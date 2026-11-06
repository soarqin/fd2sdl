"""B6三场核心动作的预声明SFX读集；不采集、不按实际波形拟合参数。

依据field_physical_exchange @0x1548e→2e2b0/2ebe1/314de；
field_magic_battle_animation @0x2ff01→30e9d/2b996；13为21ad9→21b18、
1c4cc的51f33/51f54/51f75表及field_actor_group_flash @0x1c2da。
25a96为sfx_play；只声明原调用顺序，不猜其ABI或完整A7保真。
"""
import hashlib
import math
from pathlib import Path
import struct

from analyze_fd2_audio import archive_entries
from fd2_item_list_oracle import EXE_SHA256
from fd2_battle_animation_oracle import derive_fixture, actor
from fd2_sfx_oracle import references
from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image

CORE_SCENARIOS=('physical-full-hit','player-magic0','player-heal13')
CANDIDATE_BANKS=(52,80,82)  # 各完整bank，共24样本，含未命中/其它动作的反候选。


def _sha(data):return hashlib.sha256(data).hexdigest()


def _clip(data,index):
    entries=archive_entries(data)
    if not 0<=index<len(entries):raise ValueError('FIGANI entry越界')
    value=entries[index]
    if len(value)<3:raise ValueError('FIGANI header截断')
    count=value[0]
    if count==0:return {'entry':index,'part_count':0,'entry_sha256':_sha(value),'parts':[]}
    if len(value)<8+count*4 or value[2]>count:raise ValueError('FIGANI目录/intro越界')
    offsets=[struct.unpack_from('<I',value,8+4*i)[0] for i in range(count)]+[len(value)]
    if any(start<8+count*4 or end<=start or end-start<9 for start,end in zip(offsets,offsets[1:])):
        raise ValueError('FIGANI part目录不是完整原记录')
    return {'entry':index,'part_count':count,'split_mode':value[1],'intro_end':value[2],
        'bank_selector':value[4],'entry_sha256':_sha(value),
        'parts':[{'part':i,'offset':at,'effect_marker':value[at+4],'cue':value[at+5],
                  'duration':value[at+6],'metadata_hex':value[at:at+9].hex()}
                 for i,at in enumerate(offsets[:-1])]}


def _image(executable):
    data=executable.read_bytes()
    if _sha(data)!=EXE_SHA256:raise ValueError('SFX声明仅支持完整已确认FD2.EXE')
    le=parse_le(data)
    return build_relbase_image(data,le,parse_objects(data,le))


def declare(original,scenario):
    if scenario not in CORE_SCENARIOS:raise ValueError('仅声明B6核心三场，不猜其它FX音序')
    original=Path(original);encoded,source,fixture=derive_fixture(original,scenario)
    fdother=original/'FDOTHER.DAT';data=fdother.read_bytes();banks=archive_entries(data)
    candidates=[{'bank':bank,'sample':i} for bank in CANDIDATE_BANKS for i in range(len(archive_entries(banks[bank])))]
    if len(candidates)>32:raise ValueError('完整候选bank超出既有SFX oracle上限')
    refs=[{k:v for k,v in row.items() if k!='values'} for row in references(fdother,candidates)]
    aliases={}
    for ref in refs:aliases.setdefault(ref['sha256'],[]).append({'bank':ref['bank'],'sample':ref['sample']})
    cues=[];metadata={};raw=_image(original/'FD2.EXE')
    def cue(name,bank,sample,basis,**extra):
        if (bank,sample) not in {(row['bank'],row['sample']) for row in refs}:raise ValueError('cue不在原候选bank')
        ref=next(row for row in refs if (row['bank'],row['sample'])==(bank,sample))
        cues.append({'name':name,'expected':{'bank':bank,'sample':sample},'voice_sha256':ref['sha256'],
            'source_frames':ref['source_frames'],'source_duration_ms':ref['source_frames']*1000/11025,
            'basis':basis,**extra})
    figani=(original/'FIGANI.DAT').read_bytes();caster=actor(source,fixture['caster_raw'])
    if scenario=='physical-full-hit':
        clip=_clip(figani,caster[7]*3+1)
        if not 1<=clip.get('bank_selector',0)<=6:raise ValueError('无已证物理音效bank selector')
        bank=47+clip['bank_selector'];metadata={'attack':clip,'resolved_bank':bank}
        target=actor(source,11)
        if caster[7]!=0 or struct.unpack_from('<H',target,0x40)[0]!=1 or struct.unpack_from('<H',caster,0x4c)[0]!=200:
            raise ValueError('单次必杀物理音序需要本场完整固定前置')
        for part in clip['parts']:
            if part['cue']:
                cue('attack-part-%02d'%part['part'],bank,part['cue'],'314de bank=47+selector;2ebe1 hit cue',
                    part=part['part'],effect_marker=part['effect_marker'],duration_ticks=part['duration'])
        metadata['single_attack_basis']='HP1必杀后2ebe1 v29==0清后续attack，非反击/双击音序承诺'
    elif scenario=='player-magic0':
        clip=_clip(figani,caster[7]*3+2)
        if clip['part_count']==0:clip=_clip(figani,caster[7]*3+1)
        if caster[7]!=9:raise ValueError('此声明仅已证unit9 caster')
        metadata={'casting':clip,'ignored_actor_part_cues':'30e9d仅magic IDs24/28..30转actor bank；本场magic ID0不执行',
            'FX_entry':'2b996','cue_independent_of_hit':True}
        for part in clip['parts'][:clip['intro_end']]:
            if part['effect_marker']==1:cue('cast-part-%02d'%part['part'],82,0,'30e9d part+4==1后sfx_play(54153,0)',part=part['part'])
        for particle in range(7):
            cue('fx0-particle-%02d'%particle,82,1,'2b996 phase4:state[k]==3；phase3 state=-2*k；phase5++',
                main_tick_zero_based=3+2*particle,particle=particle,repeated_voice=True,
                replacement_risk='每两次main tick重播，尾2ff01 sample-1停止；不保证整样本播完')
    else:
        tables={name:{'canonical_va':hex(va),'raw_hex':raw[va:va+33].hex(),'sha256':_sha(raw[va:va+33]),'id13_value':raw[va+13]}
            for name,va in (('start',0x51f33),('count',0x51f54),('cue',0x51f75))}
        if [tables[key]['id13_value'] for key in ('start','count','cue')]!=[57,7,12]:raise ValueError('原ID13图/音效表不符')
        metadata={'map_effect_tables':tables}
        cue('heal-transition',80,11,'21ad9先sfx11再21eb1过渡',replacement_risk='下一map cue可替换，整样本是否播完未证明')
        cue('heal-frame-57',80,12,'21b18→1c4cc；51f75[13]==12且i==0',replacement_risk='七帧后group flash替换，整样本是否播完未证明')
        cue('heal-group-flash',80,1,'21b18→1c2da入口固定sfx1')
    repeats={}
    for item in cues:repeats.setdefault((item['expected']['bank'],item['expected']['sample']),[]).append(item['name'])
    return {'schema':'fd2-b6-sfx-declaration-v1','ok':False,'declaration_only':True,'scenario':scenario,
        'source':{'fdother_sha256':_sha(data),'figani_sha256':_sha(figani),'exe_sha256':EXE_SHA256,
                  'fixture_sha256':_sha(encoded),'caster_record_hex':caster.hex()},
        'address_view':'canonical relbase VA; object1 OFF=VA-0x10000','metadata':metadata,
        'candidate_banks':list(CANDIDATE_BANKS),'candidates':candidates,'references':refs,
        'identical_voice_bytes':[{'sha256':digest,'aliases':items} for digest,items in aliases.items() if len(items)>1],
        'expected_action_cue_subsequence':cues,
        'repeated_cues':[{'bank':key[0],'sample':key[1],'names':names} for key,names in repeats.items() if len(names)>1],
        'search_windows':None,'reference_prefix_ms':None,'identity_thresholds':None,
        'actual_recording_analyzed':False,'complete_playback_or_all_recording_cues_claimed':False,
        'native_absolute_input_latency':'pending_no_first_packet_QPC','full_A7_fidelity_pass':False}


def request(declaration,windows,*,min_score,identity_margin):
    """仅把调用者预声明的完整时窗交既有oracle；不读取WAV挑窗/阈值/前缀。"""
    cues=declaration['expected_action_cue_subsequence']
    if set(windows)!={cue['name'] for cue in cues}:raise ValueError('必须显式覆盖每个声明cue，不能删重复项')
    if (type(min_score) not in (int,float) or not math.isfinite(min_score) or not 0<min_score<=1 or
        type(identity_margin) not in (int,float) or not math.isfinite(identity_margin) or not 0<=identity_margin<=1):
        raise ValueError('必须提供有限、预声明身份阈值')
    queries=[]
    for cue in cues:
        bounds=windows[cue['name']]
        if (not isinstance(bounds,(list,tuple)) or len(bounds)!=2 or any(type(v) not in (int,float) or not math.isfinite(v) for v in bounds)
            or not 0<=bounds[0]<bounds[1]):raise ValueError('显式时窗无效')
        queries.append({'name':cue['name'],'start_ms':bounds[0],'end_ms':bounds[1],'expected':cue['expected']})
    return {'schema':'fd2-sfx-query-v1','candidates':declaration['candidates'],'queries':queries,
        'min_score':min_score,'identity_margin':identity_margin,'prefix_policy':'full independent VOICE; no recorded-wave fitting'}


def order_observation(declaration,observation):
    """只报告唯一录音内时点；同字节身份/重复峰/缺项保守pending，不排序造顺序。"""
    if (observation.get('schema')!='fd2-sfx-observation-v1' or observation.get('fdother',{}).get('sha256')!=declaration['source']['fdother_sha256']
        or observation.get('references')!=declaration['references']):raise ValueError('观测不是同一完整独立VOICE读集')
    expected=declaration['expected_action_cue_subsequence'];queries=observation.get('queries',[])
    result={'status':'pending_capture_or_unique_cues','ok':False,'recording_relative_only':True,
        'native_absolute_input_latency':'pending_no_first_packet_QPC','full_A7_fidelity_pass':False,'cues':[],'intervals_ms':[]}
    if not queries:return result
    if [(q.get('name'),q.get('expected')) for q in queries]!=[(cue['name'],cue['expected']) for cue in expected]:
        raise ValueError('观测删改/重排声明cue')
    aliased={row['sha256'] for row in declaration['identical_voice_bytes']};times=[]
    for cue,row in zip(expected,queries):
        status='pending_identity_or_occurrence'
        if cue['voice_sha256'] in aliased:status='pending_identical_bytes_bank_or_sample'
        elif row.get('identity_status')=='supported_candidate' and row.get('onset_status')=='observed_wav_position':
            hit=row.get('candidates',[{}])[0]
            if (hit.get('bank'),hit.get('sample'))!=(cue['expected']['bank'],cue['expected']['sample']):raise ValueError('观测最佳candidate与声明不符')
            at=hit.get('onset_ms')
            if type(at) not in (int,float) or not math.isfinite(at) or at<0:raise ValueError('录音内时点无效')
            times.append(at);status='observed_unique_wav_onset'
        result['cues'].append({'name':cue['name'],'status':status})
    if len(times)==len(expected):
        if all(a<b for a,b in zip(times,times[1:])):
            result.update(status='observed_finite_recording_order_only',intervals_ms=[b-a for a,b in zip(times,times[1:])])
        else:result['status']='pending_reused_peak_or_wrong_order'
    return result
