"""2b4fb 选择页独立原资源ROI：完整label与310x99网格，不依赖SDL图表。"""
from PIL import Image

import fd2_g3_prepare_oracle as model
import fd2_g3_prepare_pages as pages
from fd2_magic_list_oracle import number
from run_original_player import sheet_frame,exact_tile


def selection(assets,state,chosen,cursor,quota):
    if quota not in (15,19) or not 0<=cursor<state['count']-1 or len(set(chosen))!=len(chosen) or \
        any(not 1<=raw<state['count'] for raw in chosen) or len(chosen)>quota:
        raise ValueError('G3 deployment首轮选择域无效')
    label=pages._image(pages.member(assets['info'],137),assets['palette'],size=(86,86))
    number(label,assets['info'],quota,61-5,35-7,31,2)
    number(label,assets['info'],quota-len(chosen),61-5,73-7,42,2)
    ids=list(dict.fromkeys(model.record(state,i)[7] for i in range(state['count'])))
    if len(ids)!=state['count']:raise ValueError('本原生代表要求已定义的唯一cache目录；不略掉未知sprite来签通过')
    background=pages._image(pages.member(assets['info'],21),assets['palette'],lmi=True,size=(310,99))
    cursor_image=sheet_frame(pages.archive_entry(assets['other'],1),0)
    result=[]
    for phase in (0,1,2):
        canvas=Image.new('P',(320,200),0);canvas.putpalette(assets['palette']);canvas.paste(background,(5,94))
        pages._paste(canvas,cursor_image,(23+28*(cursor%10),104+30*(cursor//10)),zero=True)
        for i in range(state['count']-1):
            raw=i+1;pages._sprite(assets,state,raw,phase)  # 原RLE完整/opaque0检查，不用其mode2输出。
            image=sheet_frame(assets['icons'],12*ids[raw]+phase);image.putpalette(assets['palette'])
            if raw not in chosen:
                image=image.point(lambda pixel:(pixel&7)+24 if pixel else 0);image.putpalette(assets['palette'])
            pages._paste(canvas,image,(23+28*(i%10),100+30*(i//10)+(3 if raw in chosen else 0)),zero=True)
        result.append(canvas.crop((5,94,315,193)))
    return {'label':label,'grids':result,'chosen':list(chosen),'cursor':cursor,'quota':quota,
        'remaining':quota-len(chosen),'source_cva':['0x2af28','0x2b4fb','0x1297d','0x187d6']}


def match(frame,expected):
    label=exact_tile(frame,expected['label'],(5,7))
    if not label:return None
    for phase,grid in enumerate(expected['grids']):
        hit=exact_tile(frame,grid,(5,94))
        if hit:return {'kind':'deployment_selection','cursor':expected['cursor'],'chosen':expected['chosen'],
            'quota':expected['quota'],'remaining':expected['remaining'],'sprite_phase':phase,
            'label_roi':[5,7,86,86],'grid_roi':[5,94,310,99],'checked_pixels':86*86+310*99,
            'full_cursor_selected_gray_sprites_included':True,'source_cva':expected['source_cva']}
    return None
