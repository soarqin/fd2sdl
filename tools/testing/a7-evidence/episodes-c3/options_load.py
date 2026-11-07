"""A7 私有正式配置/Load接线与取证；默认只读dry，不作声音保真通过声明。"""
from __future__ import annotations
import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import shutil
import sys

sys.dont_write_bytecode = True
META = 0x30C3
FIXTURE_SHA = 'ef7c27c00e15835866622d53ee734fec008e70a11fbc5f1780421d4dbbd202c1'
FIXTURE_META = bytes((1,12,0,5,11,6,14,1,3,4,0,0,0,0,0,1,1,1))
EXPECTED_MANIFEST_SHA = 'ab064004fe3f7819d2a1b69c6487eb6e4702300065759132157bd51768c19ab7'
BIN_SHA = dict(game='1c0a1431b4faf6805d289e1eea6623963c96c5d6be6950668f66e354bd63f4f6',
    driver='31f8ad50d3b711189ab0b61562a367ef761526901b94112de9497285254eddf4',
    dosbox='a82adcfa7af2784566cfeff74c077cefed08505396db2948b7e6aa7d4cee0832')


def fact(path):
    path = Path(path).resolve(strict=True)
    before = path.stat(); data = path.read_bytes(); after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise AssertionError('input changed while reading: ' + str(path))
    return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(),
                size=len(data), mtime_ns=after.st_mtime_ns)


def assert_options_only(before, after, values):
    """仅正式Save测量：整plain包含未知尾，只允许两配置字节与checksum改变。"""
    if len(before) != 0x59CB or len(after) != len(before):
        raise AssertionError('Save size')
    if sum(after[:-4]) != int.from_bytes(after[-4:], 'little'):
        raise AssertionError('Save checksum')
    if after[META+16:META+18] != bytes(values):
        raise AssertionError('measured Music/SFX differs')
    expected = bytearray(before)
    expected[META+16:META+18] = bytes(values)
    expected[-4:] = sum(expected[:-4]).to_bytes(4, 'little')
    if after != expected:
        raise AssertionError('non-option Save bytes changed')


def options_match(frame, oracle, values, selected, kind, offsets):
    if kind not in ('original', 'sdl') or selected not in (0, 1):
        raise ValueError('finite options scope')
    commands = (18 + (not values[0]), 20 + (not values[1]),
                22 + values[2], 24 + (not values[3]))
    matches = []
    for index, command in enumerate(commands):
        at = tuple(a+b for a,b in zip(oracle.focus, offsets[index]))
        if kind == 'sdl' and index >= 2:
            # 原disabled全透明，不能当正像素；两种值的normal/bright均须不存在。
            visible = (66,67,69,70) if index == 2 else (72,73,75,76)
            if oracle.choices(frame, visible, at): return None
            matches.append(dict(item=index, disabled_transparent=True, positive_pixels_compared=False))
        else:
            found = oracle.choices(frame, (command*3 + (selected == index),), at)
            if not found: return None
            matches.append(found)
    return dict(kind='options', selected=selected, values=list(values), matches=matches)


def first_selected_yes(oracle, observe_any):
    """同一有界wait：plain继续，首次明确NO即失败；不重起deadline或补键。"""
    observed = False
    def predicate(frame):
        nonlocal observed
        any_buttons = oracle.buttons(frame, None)
        if not any_buttons: return None
        if not observed:
            observe_any(frame, any_buttons); observed = True
        if oracle.buttons(frame, 1):
            raise AssertionError('first explicit selection is NO; later YES cannot wash it out')
        return oracle.buttons(frame, 0)
    return predicate


def source_manifest_facts(root, manifest):
    rows = json.loads(manifest.read_text(encoding='utf-8'))['files']
    present = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()
        and not p.relative_to(root).as_posix().startswith(('original_game/','src/Debug/','src/Release/'))}
    if present != set(rows):
        raise AssertionError('source file set changed: '+repr(dict(added=sorted(present-set(rows)),missing=sorted(set(rows)-present))))
    found = {}
    for relative, row in rows.items():
        path = (root / relative).resolve(strict=True)
        path.relative_to(root)
        actual = fact(path)
        if (actual['sha256'],actual['size'],actual['mtime_ns']) != (
                row['sha256'],row['size'],row['mtime_ns']):
            raise AssertionError('source manifest differs: '+relative)
        found[relative] = actual
    return found


def load_tools(root):
    sys.path.insert(0, str(root/'tools'))
    return {name: importlib.import_module(name) for name in (
        'run_original_comparison','run_original_battle_menu','run_original_end_turn',
        'run_graphics_tests','run_original_load','run_original_battle_animation','fd2_app_mix_capture',
        'fd2_display_requirement','inspect_dro_v2')}


def prepare(args):
    for key in ('root','manifest','game','driver','dosbox','fixture','public_app'):
        setattr(args,key,getattr(args,key).resolve(strict=True))
    args.artifacts = args.artifacts.resolve()
    if args.artifacts.exists(): raise ValueError('new artifacts directory required')
    if args.root == args.artifacts or args.root in args.artifacts.parents:
        raise ValueError('artifacts must be outside frozen source')
    for word in args.artifacts.parts:
        if word.casefold() == 'original_game': raise ValueError('readonly evidence destination')
    if fact(args.manifest)['sha256'] != EXPECTED_MANIFEST_SHA: raise ValueError('require sealed audio-head c3 manifest')
    for key,expected in BIN_SHA.items():
        if fact(getattr(args,key))['sha256'] != expected: raise ValueError('unexpected '+key+' binary')
    if args.dosbox.name.casefold() != 'dosbox-x.exe': raise ValueError('use actual pinned DOSBox EXE, not shim')
    source = source_manifest_facts(args.root,args.manifest)
    fixture = fact(args.fixture)
    if fixture['sha256'] != FIXTURE_SHA: raise ValueError('require proven focus6,14 fixture')
    tools = load_tools(args.root)
    plain = tools['run_graphics_tests'].decode_save(args.fixture)
    if plain[META:META+18] != FIXTURE_META: raise ValueError('fixture metadata changed')
    original = args.root/'original_game'
    assets = tools['run_original_end_turn'].round_assets(original)
    tools['run_original_end_turn'].validate_initial(args.fixture,
        tools['run_original_comparison'].archive_entry((original/'FDFIELD.DAT').read_bytes(),1)[0])
    inputs = {key: fact(getattr(args,key)) for key in ('manifest','game','driver','dosbox','fixture')}
    inputs['public_app'] = fact(args.public_app)
    inputs['worker'] = fact(__file__)
    inputs['passive_observer'] = fact(Path(__file__).with_name('passive_observer.py'))
    original_facts = {p.name:fact(p) for p in original.iterdir() if p.is_file()}
    if len(original_facts) != 50: raise ValueError('expected isolated original50')
    voice = tools['run_original_comparison'].archive_entry(
        tools['run_original_comparison'].archive_entry((original/'FDOTHER.DAT').read_bytes(),31),8)
    audio_scope = dict(menu_cue=dict(bank=31,sample=8,voice_bytes=len(voice),voice_sha256=hashlib.sha256(voice).hexdigest()),
        minimum_sound_gate_passed=False,finite_VOICE_identity_consumer='pending',
        music_TL_attenuation_restore_consumer='pending',default_amplitude_verified=False,
        absolute_timing_verified=False)
    return dict(tools=tools,plain=plain,assets=assets,source=source,inputs=inputs,
                original=original,original_facts=original_facts,
                audio_scope=audio_scope,
                titles=tools['run_original_comparison'].title_templates(original/'FDOTHER.DAT'))


def exercise(capture, kind, prepared, save, record, app_mix=None, *, episode='options-only'):
    tools = prepared['tools']; b = tools['run_original_battle_menu']; e = tools['run_original_end_turn']
    oracle = e.RoundOracle(prepared['assets'],(28,76))
    values = [1,1,0,1]
    guard = [save.read_bytes(),save.stat().st_mtime_ns]
    audio_active=False
    def check_audio():
        if app_mix and audio_active:app_mix.assert_running(capture.process)
    def key(name):
        check_audio();capture.key(name)
    def stop_audio():
        nonlocal audio_active
        check_audio()
        if app_mix:record['app_mix_capture']=app_mix.finish(capture.process)
        audio_active=False
    def unchanged():
        if save.read_bytes()!=guard[0] or save.stat().st_mtime_ns!=guard[1]:
            raise AssertionError('disk changed outside explicit UI Save')
    def cp(name,predicate,**kwargs):
        def checked(frame):check_audio();return predicate(frame)
        from passive_observer import wait_state as observed_wait
        check_audio();result=observed_wait(b,capture,name,checked,record['checkpoints'],
            record.setdefault('sampling_observations',[]),
            classify=lambda frame: dict(parent_any=oracle.menu(frame,None),
                parent_bright=[i for i in range(4) if oracle.menu(frame,i)]),**kwargs)
        check_audio();unchanged();return result
    def yes(label):
        def first(frame,detail): b.save_checkpoint(capture,label+'-first-any',frame,detail,record['checkpoints'])
        return cp(label,first_selected_yes(oracle,first))
    def secondary(label,index,direction):
        key('ENTER');cp(label+'-parent',lambda f:oracle.menu(f,0))
        key('UP');cp(label+'-top',lambda f:oracle.menu(f,0))
        key('ENTER');cp(label+'-secondary',lambda f:oracle.secondary(f,0))
        key(direction);cp(label+'-selected',lambda f:oracle.secondary(f,index))
        key('ENTER');yes(label+'-yes')
    def save_now(label):
        prefix='save-'+label
        secondary(prefix,1,'LEFT');previous,mtime=guard
        key('ENTER')
        plain=tools['run_original_load'].wait_measured_save(save,previous,mtime)
        guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
        cp(prefix+'-browse',oracle.closed_browse)
        snapshot=capture.artifacts/(label+'.SAV');shutil.copy2(save,snapshot)
        record['saves'].append(dict(name=label,snapshot=fact(snapshot),live_mtime_ns=guard[1]))
        return plain
    def open_options(label):
        key('ENTER');cp(label+'-open-parent',lambda f:oracle.menu(f,0))
        key('RIGHT');cp(label+'-open-right',lambda f:oracle.menu(f,2))
        key('ENTER');cp(label+'-options',lambda f:options_match(f,oracle,values,0,kind,b.MENU_OFFSETS))
    def close_options(label):
        key('ESC');cp(label+'-close-parent',lambda f:oracle.menu(f,0))
        key('ESC');cp(label+'-close-browse',oracle.closed_browse)
    def toggle(label,index):
        open_options(label)
        if index==1:
            key('LEFT');cp(label+'-sfx-selected',lambda f:options_match(f,oracle,values,1,kind,b.MENU_OFFSETS))
        record['events'].append(dict(name=label+'-before',clock=tools['run_original_battle_animation'].qpc_stamp(),values=list(values)))
        key('ENTER');values[index]=int(not values[index])
        cp(label+'-changed',lambda f:options_match(f,oracle,values,index,kind,b.MENU_OFFSETS))
        record['events'].append(dict(name=label+'-after',clock=tools['run_original_battle_animation'].qpc_stamp(),values=list(values)))
        close_options(label)
    if episode not in ('options-only','load-only'):raise ValueError('unknown finite SDL episode')
    b.wait_title(capture,prepared['titles'][3][0]);key('UP');capture.wait(prepared['titles'][3][2]);key('ENTER')
    cp('continued-primary19-browse',oracle.closed_browse,timeout=45)
    baseline=save_now('baseline')
    # 首次Load重建的active +2 cache不是业务字段；沿既有完整Load测量合同单列。
    record['initial_load_measurement']=tools['run_original_load'].check_load_measurement(prepared['plain'],baseline,[])
    if app_mix:
        record['app_mix_ready']=app_mix.start(capture.process);audio_active=True
    record['episode']=episode
    if episode=='options-only':
        for name,index in (('music-off',0),('music-on',0),('sfx-off',1),('sfx-on',1)):
            toggle(name,index)
            if name=='sfx-on':stop_audio()  # 最后一轮Save仅测量，不再占录音窗。
            measured=save_now(name)
            assert_options_only(baseline,measured,values[:2])
        record['business_sequence_completed']=True
        return
    # Disk A=(1,1), live B=(0,0): cancels retain B; YES restores A without rewriting disk.
    toggle('before-load-music-off',0);toggle('before-load-sfx-off',1)
    for branch in ('escape','no'):
        secondary('load-'+branch,2,'RIGHT')
        if branch=='no': key('RIGHT');cp('load-no-highlight',lambda f:oracle.buttons(f,1))
        key('ESC' if branch=='escape' else 'ENTER')
        cp('load-'+branch+'-browse',oracle.closed_browse,transition=oracle.cancellation_notice)
        open_options('load-'+branch+'-flags-retained');close_options('load-'+branch+'-flags-retained')
        record['cancellations'].append(dict(branch=branch,live_options_observed=[0,0],disk_bytes_mtime_unchanged=True,full_runtime_snapshot_measured=False))
    secondary('load-yes',2,'RIGHT');key('ENTER')
    cp('loaded-A-browse',oracle.closed_browse,timeout=45)
    values[:2]=[1,1];open_options('loaded-A-flags');close_options('loaded-A-flags')
    stop_audio()  # 已证明Load后配置；最终完整Save留在录音窗之外。
    final=save_now('loaded-A-final');assert_options_only(baseline,final,(1,1))
    if final!=baseline: raise AssertionError('Load did not restore full baseline')
    record['business_sequence_completed']=True


def finish_guards(args, prepared, report):
    """每个晚守卫独立收错；原业务首错与target receipts始终留在同一报告。"""
    checks = {
        'source_unchanged': lambda: source_manifest_facts(args.root,args.manifest)==prepared['source'],
        'original50_unchanged': lambda: {p.name:fact(p) for p in prepared['original'].iterdir() if p.is_file()}==prepared['original_facts'],
        'fixture_bins_worker_manifest_unchanged': lambda: {k:fact(v['path']) for k,v in prepared['inputs'].items()}==prepared['inputs'],
        'closed_capture_files_unchanged': lambda: all(fact(row['path'])==row for case in report.get('cases',[]) for row in case.get('closed_capture_facts',{}).values()),
    }
    report.setdefault('guards',{});report.setdefault('guard_errors',{})
    for key,check in checks.items():
        try:
            current=check()
            report['guards'][key]=report['guards'].get(key,True) and current
            if not current:report['guard_errors'].setdefault(key,{'type':'GuardMismatch','message':'SHA/size/mtime or file set changed'})
        except BaseException as error:
            report['guards'][key]=False
            report['guard_errors'].setdefault(key,{'type':type(error).__name__,'message':str(error)})
    if not all(report['guards'].values()):
        report['collection_passed']=False
        report.setdefault('error',{'type':'LateGuardFailure','message':'see independent guard diagnostics'})


def execute(args, prepared, *, _final_report=None, scenario='sdl-options-only'):
    tools=prepared['tools']; common=tools['run_original_comparison']; battle=tools['run_original_battle_animation']
    battle.owned_job_required()
    if args.artifacts.exists(): raise ValueError('previous output')
    args.artifacts.mkdir(parents=True,exist_ok=False)
    report=dict(schema='fd2-a7-options-load-collection-v1',ok=False,collected_only=True,
        status='failed',collection_passed=False,entire_A7_passed=False,sound_semantics_verified=False,
        audio_declaration=prepared['audio_scope'],
        scenario=scenario,inputs=prepared['inputs'],cases=[],processes=[],checkpoints=[],evidence_scope='resource business checkpoints and closed audio; not waveform/absolute timing')
    if scenario not in ('sdl-options-only','sdl-load-only','opening18-to11','one-END19-12-19'):raise ValueError('unknown finite scenario')
    roles=('sdl',) if scenario in ('sdl-options-only','sdl-load-only') else ('original','sdl')
    report['executed_roles']=list(roles)
    report['native_executed_this_job']='original' in roles
    if roles==('sdl',):report['prior_native_reference']=prepared['native_reference']['summary']
    if _final_report is not None:_final_report['value']=report
    try:
        for kind in roles:
            work=args.artifacts/kind;work.mkdir()
            record=dict(kind=kind,status='failed',checkpoints=[],inputs=[],saves=[],events=[],cancellations=[])
            report['cases'].append(record)
            removed=[k for k in os.environ if k.upper() in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_SAVE_PATH','FD2SDL_TEST_AUDIO_QUIET') or k.upper().startswith(('FD2SDL_APP_PCM_CAPTURE_','DOSBOX_'))]
            env={k:v for k,v in os.environ.items() if k not in removed}
            record['child_environment_removed_names']=sorted(removed)
            mix=None
            source_save=prepared['original']/'FD2.SAV' if scenario=='opening18-to11' else args.fixture
            record['source_save']=fact(source_save)
            if kind=='original':
                game=work/'game';shutil.copytree(prepared['original'],game,copy_function=shutil.copy2)
                save=game/'FD2.SAV';shutil.copy2(source_save,save);captures=work/'captures';captures.mkdir()
                config=work/'dosbox.conf'
                config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\nshow recorded filename=false\ncaptures='+str(captures)+'\n[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nDX-CAPTURE /O /A /-D FD2.EXE\nexit\n',encoding='utf-8')
                command,cwd=[str(args.dosbox),'-conf',str(config),'-noconsole'],work
                record['isolated_game_before']={p.name:fact(p) for p in game.iterdir() if p.is_file()}
            else:
                save=work/'FD2.SAV';shutil.copy2(source_save,save);folder=work/'app-mix';folder.mkdir()
                env.update(FD2SDL_SAVE_PATH=str(save),FD2SDL_APP_PCM_CAPTURE_DIR=str(folder),FD2SDL_APP_PCM_CAPTURE_MAX_MS='120000',FD2SDL_APP_PCM_CAPTURE_QUIET='1')
                command,cwd=[str(args.game)],args.root
            with common.running(command,cwd,env,work/'game.log',args.driver,report['processes']) as process:
                record['pid']=process.pid;capture=common.Capture(args.driver,process,work,record['inputs'])
                if kind=='sdl':mix=tools['fd2_app_mix_capture'].AppMixCapture(folder,process.pid,120000)
                try:
                    if scenario in ('sdl-options-only','sdl-load-only'):
                        exercise(capture,kind,prepared,save,record,mix,episode='options-only' if scenario=='sdl-options-only' else 'load-only')
                    elif scenario=='opening18-to11':
                        from short_scenes import exercise_opening
                        exercise_opening(capture,kind,prepared,save,record,mix)
                    else:
                        from short_scenes import exercise_end
                        exercise_end(capture,kind,prepared,save,record,mix)
                    record['completed_live_save_fact']=fact(save)
                finally:
                    if mix:mix.request_stop()
            if fact(save)!=record['completed_live_save_fact']:
                raise AssertionError('normal close changed the last verified live Save')
            if kind=='original':
                after={p.name:fact(p) for p in game.iterdir() if p.is_file()}
                before=record['isolated_game_before'];mutable={'FD2.SAV','FD2.TMP'}
                record['isolated_static_unchanged']=all(after.get(k)==v for k,v in before.items() if k.upper() not in mutable) and all(k in before or k.upper() in mutable for k in after)
                record['isolated_game_after']=after
                if not record['isolated_static_unchanged']:raise AssertionError('isolated original static resource changed')
                dro=list(captures.glob('*.dro'));wav=list(captures.glob('*.wav'))
                if len(dro)!=1 or len(wav)!=1:raise AssertionError('one native DRO/WAV required')
                parsed=tools['inspect_dro_v2'].inspect(dro[0])
                if parsed.get('ok') is not True or parsed.get('writer_consistent') is not True:raise AssertionError('native DRO not closed/writer-consistent')
                record['native_dro']=parsed;record['audio_wave']=battle.wav_info(wav[0])
            else:
                wave=battle.wav_info(folder/'app-mix.wav',expected_rate=48000)
                summary=record['app_mix_capture']['summary']
                if wave['frames']!=summary['frames'] or wave['data_bytes']!=summary['data_bytes']:raise AssertionError('app-mix wave/summary differ')
                record['audio_wave']=wave
            record['closed_capture_facts']={p.relative_to(work).as_posix():fact(p) for p in work.rglob('*')
                if p.is_file() and p.suffix.lower() in ('.png','.bmp','.wav','.dro','.sav')}
            if roles==('sdl',):
                from native_reference import compare_episode
                record['native_reference_comparison']=compare_episode(record,prepared['native_reference'],prepared)
            finish_guards(args,prepared,report)
            record['after_close_guards']=dict(report['guards'])
            if not all(report['guards'].values()):
                record['status']='failed'
                raise AssertionError('endpoint closed but late source/capture guards failed; next endpoint not started')
            record['status']='passed'
        if len(report['processes'])!=len(roles) or any(not p.get('cleaned') or p.get('forced_cleanup') or p.get('exit_code')!=0 for p in report['processes']):
            raise AssertionError('normal owned target cleanup failed')
        report['collection_passed']=True
    except BaseException as error:
        report.setdefault('error',{'type':type(error).__name__,'message':str(error)})
        if isinstance(error,(KeyboardInterrupt,SystemExit)):raise
    finally:
        finish_guards(args,prepared,report)
        report['status']='collection-passed-sound-pending' if report['collection_passed'] else 'failed'
        (args.artifacts/'comparison-body.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','manifest','game','driver','dosbox','fixture','artifacts'):parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--public-app',type=Path,default=Path('E:/Projects/fd2sdl/src/Debug/fd2sdl.exe'))
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args(argv);prepared=prepare(args)
    if not args.execute:
        print(json.dumps(dict(dry=True,GUI_started=False,device_opened=False,real_display_API_called=False,
            inputs=prepared['inputs'],source_files=len(prepared['source']),original_files=len(prepared['original_facts']),fixture_metadata=list(FIXTURE_META),artifacts=str(args.artifacts))))
        return 0
    display={};report=None;error=None;partial={}
    try:
        with prepared['tools']['fd2_display_requirement'].windows_display_requirement(display):
            report=execute(args,prepared,_final_report=partial)
    except BaseException as caught:error=caught
    if report is None:report=partial.get('value')
    if report is None:report=dict(ok=False,collected_only=True,collection_passed=False,entire_A7_passed=False)
    if error:
        report['collection_passed']=False;report['status']='failed';report['entry_error']={'type':type(error).__name__,'message':str(error)}
    report['display_requirement']=display
    if 'value' in partial:
        try:
            with (args.artifacts/'comparison.json').open('x',encoding='utf-8') as stream:json.dump(report,stream,ensure_ascii=False,indent=2)
        except OSError as caught:
            report['collection_passed']=False
            report['status']='failed'
            report['report_write_error']={'type':type(caught).__name__,'message':str(caught)}
    print(json.dumps(report,ensure_ascii=False))
    return 0 if report['collection_passed'] else 130 if isinstance(error,KeyboardInterrupt) else 1


if __name__=='__main__':raise SystemExit(main())
