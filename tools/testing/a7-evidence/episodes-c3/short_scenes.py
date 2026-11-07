"""两个明确短场景；复用原资源与END执行器，不新增捕获或音乐后端。"""
from collections import Counter
import importlib
from pathlib import Path
import shutil
import time

import options_load as common


def prepare_short(prepared):
    opening=importlib.import_module('fd2_opening_oracle')
    runner=importlib.import_module('run_original_opening')
    pages=opening.build_pages(prepared['original'],prepared['assets'],stage_plan=((32,3),))
    prefix=[p for p in pages if p['fragment'] in (0,1)]
    first=next(p for p in pages if p['fragment']==2)
    if Counter(p['fragment'] for p in prefix)!={0:7,1:22} or len(prefix)!=29:
        raise AssertionError('opening29 exact wait contract')
    if (first['stage'],first['fragment'],first['token_offset'])!=(32,2,41):
        raise AssertionError('first fragment2 wait changed')
    source=prepared['tools']['run_graphics_tests'].decode_save(prepared['original']/'FD2.SAV')
    counts=runner.title_menu_counts(source)
    banner=prepared['tools']['run_original_end_turn'].round_two_template(prepared['original']/'FDOTHER.DAT',prepared['assets']['palette'])
    return dict(opening=opening,prefix=prefix,first=first,title_counts=counts,banner=banner,
        prefix_description=[{k:p[k] for k in ('stage','fragment','token_offset','reason')} for p in prefix])


def observe_audio(capture,record,label,seconds=2):
    """已ACK状态上的声明听取窗；仅存活/源guard，不作输入settle或FPS判据。"""
    started=time.monotonic();deadline=started+seconds
    while time.monotonic()<deadline:
        if capture.process.poll() is not None:raise RuntimeError('target exited during declared audio observation')
        time.sleep(min(.02,max(0,deadline-time.monotonic())))
    record['events'].append(dict(name=label,observation_seconds=time.monotonic()-started,
        requested_seconds=seconds,extra_keys=0,frames_or_FPS_claimed=False))


def exercise_opening(capture,kind,prepared,save,record,mix):
    b=prepared['tools']['run_original_battle_menu'];short=prepared['short'];o=short['opening']
    before,stamp=save.read_bytes(),save.stat().st_mtime_ns
    def unchanged():prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
    def cp(label,page):
        result=b.wait_state(capture,label,lambda frame:o.page_match(frame,page,prepared['assets']),record['checkpoints'],timeout=90)
        unchanged();return result
    count=short['title_counts'][kind]
    b.wait_title(capture,prepared['titles'][count][0]);unchanged()
    capture.key('ENTER')  # 默认START；不是CONTINUE，也不导入旧档。
    acknowledgements=[]
    for ordinal,page in enumerate(short['prefix'],1):
        cp(f'opening-{ordinal:02d}-fragment{page["fragment"]}',page)
        if ordinal==29:
            if mix:record['app_mix_ready']=mix.start(capture.process)
            observe_audio(capture,record,'before-opening-fade-track18');unchanged()
        capture.key('ENTER');acknowledgements.append(dict(ordinal=ordinal,fragment=page['fragment'],token_offset=page['token_offset']))
    cp('opening-first-fragment2-track11-window',short['first'])
    observe_audio(capture,record,'first-fragment2-track11');unchanged()
    if mix:record['app_mix_capture']=mix.finish(capture.process)
    record['opening_acknowledgements']=acknowledgements
    record['terminal_wait_acknowledged']=False
    record['source_save_unchanged']=True
    record['business_sequence_completed']=len(acknowledgements)==29


def exercise_end(capture,kind,prepared,save,record,mix):
    """只借原exercise的明确检查点放观察器；原按键/phase/Save业务流程不改。"""
    e=prepared['tools']['run_original_end_turn'];b=prepared['tools']['run_original_battle_menu']
    before,stamp=save.read_bytes(),save.stat().st_mtime_ns
    original_wait=e.wait_state
    default_oracle=e.RoundOracle(prepared['assets'],(28,76))
    observer={'started':False,'stopped':False,'default_yes_frames':[]}
    def watched(cap,name,predicate,report,**kwargs):
        if name.endswith('default-yes'):
            def first(frame,detail):
                b.save_checkpoint(cap,name+'-first-any',frame,detail,report)
                observer['default_yes_frames'].append(name+'-first-any')
            predicate=common.first_selected_yes(default_oracle,first)
        result=original_wait(cap,name,predicate,report,**kwargs)
        if name.endswith('initial-browse'):
            prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
            if mix:record['app_mix_ready']=mix.start(cap.process)
            observer['started']=True
            observe_audio(cap,record,'primary19-before-END',3)
            prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
        if name.endswith('new-player-browse'):
            prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
            observe_audio(cap,record,'returned-primary19-before-audio-stop',3)
            prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
            if mix:record['app_mix_capture']=mix.finish(cap.process)
            observer['stopped']=True
        return result
    e.wait_state=watched
    try:
        measured=e.exercise(capture,kind,prepared['assets'],(28,76),prepared['titles'],
            prepared['short']['banner'],save,capture.artifacts/'game.log',record['checkpoints'],initial_steps=())
    finally:e.wait_state=original_wait
    if not observer['started'] or not observer['stopped']:raise AssertionError('END audio window did not bracket exactly one round')
    metadata=prepared['tools']['run_original_comparison'].archive_entry((prepared['original']/'FDFIELD.DAT').read_bytes(),1)
    record['measurement']=e.check_saved_round(prepared['plain'],measured,len(metadata))
    snapshot=capture.artifacts/'round002-measured.SAV';shutil.copy2(save,snapshot)
    record['saves'].append(dict(name='round002',snapshot=common.fact(snapshot),live_mtime_ns=save.stat().st_mtime_ns))
    record['end_observer']=observer
    record['business_sequence_completed']=True
