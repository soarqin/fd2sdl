"""自然新游戏的纯资源门；不运行进程、不读写活动存档。

依据new_game_opening_play @0x3231b、text_dialog_render_tokens @0x15f84、
sub_16C57 @0x16c57、dialog_text_scroll_up @0x16e24、field_movement_script_ptr
@0x4eb48（原登记同名）。原版117等待已独立WGC校准，玩法配对仍由runner执行。
"""
import hashlib
import struct

from PIL import Image, ImageOps

from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image
from dump_fd2_fixup_table import collect_internal_offset_fixups
from run_original_comparison import archive_entry
from run_original_battle_menu import match_indexed
from run_original_end_turn import META, UNITS, STRIDE, browse_anywhere, decode_lmi
from run_original_player import exact_tile


def fragments(data, stage, count):
    text = archive_entry(data, stage + 1)
    entries = struct.unpack_from("<H", text)[0] // 2
    if count > entries:
        raise ValueError("opening fragment directory truncated")
    for fragment in range(count):
        start = struct.unpack_from("<H", text, fragment * 2)[0]
        end = struct.unpack_from("<H", text, (fragment + 1) * 2)[0] if fragment + 1 < entries else len(text)
        if not 2 * entries <= start < end <= len(text) or (end - start) % 2:
            raise ValueError("opening fragment bounds invalid")
        yield fragment, list(struct.unpack("<" + "h" * ((end - start) // 2), text[start:end]))


def raw_ui(sheet,index):
    start,end = struct.unpack_from("<II",sheet,6+index*4)
    data = sheet[start:end]
    width,height = struct.unpack_from("<HH",data)
    if len(data)!=4+width*height:
        raise ValueError("not an exact raw UI frame")
    return Image.frombytes("P",(width,height),data[4:])


def portraits(original,identity,palette,bottom):
    entry = archive_entry((original/"DATO.DAT").read_bytes(),identity)
    images = []
    for frame in (0,3):
        start = struct.unpack_from("<I",entry,4*frame)[0]
        end = struct.unpack_from("<I",entry,4*(frame+1))[0] if frame<3 else len(entry)
        if not 16<=start<end<=len(entry):
            raise ValueError("DATO frame bounds invalid")
        image = decode_lmi(entry[start:end])
        if image.size!=(80,80):
            raise ValueError("DATO portrait not80x80")
        if bottom:
            image = ImageOps.mirror(image)
        image.putpalette(palette)
        images.append(image)
    return images


def build_pages(original,assets):
    """精确维护16e24的像素mask；只要求已知文字写集，不猜空白背景。"""
    text_data = (original/"FDTXT.DAT").read_bytes()
    other = (original/"FDOTHER.DAT").read_bytes()
    field = (original/"FDFIELD.DAT").read_bytes()
    font,sheet = archive_entry(other,4),archive_entry(other,5)
    arrows = [raw_ui(sheet,index) for index in (18,19)]
    for image in arrows:
        image.putpalette(assets["palette"])
    pages = []
    for stage,count in ((32,6),(31,10),(0,3)):
        metadata = archive_entry(field,3*stage+1)
        templates = [metadata[i:i+26] for i in range(0x83,len(metadata),26)]
        identities = ([0,9,4,30]+[t[1] for t in templates[:8]]) if stage==0 else [t[1] for t in templates]
        for fragment,tokens in fragments(text_data,stage,count):
            canvas = Image.new("P",(320,200),0)
            canvas.putpalette(assets["palette"])
            speaker = None
            x = y = ox = oy = row = 0
            def pause(reason,index,continuation=False):
                expected = canvas.copy()
                if continuation:
                    expected.paste(0,(ox+96,oy+57,ox+112,oy+73))
                known = sum(pixel!=0 for pixel in expected.getdata())
                if known<256:
                    raise ValueError("too few known text pixels at wait")
                control,argument = speaker
                identity = identities[argument] if control in (-19,-20) else argument
                if control in (-17,-18) and argument!=39:
                    identity = next((i for i in identities if i==argument),argument)
                bottom = control in (-18,-20)
                pages.append({"stage":stage,"fragment":fragment,"token_offset":index,
                    "reason":reason,"speaker_control":control,"speaker_argument":argument,
                    "portrait_id":identity,"portrait_position":(8,115) if bottom else (232,5),
                    "portraits":portraits(original,identity,assets["palette"],bottom),
                    "border_position":(5,112) if bottom else (5,2),
                    "text":expected,"known_text_pixels":known,
                    "arrow_position":(ox+96,oy+62) if continuation else None,"arrows":arrows})
            index = 0
            while index<len(tokens):
                token = tokens[index]
                if token in (-17,-18,-19,-20):
                    if speaker is not None:
                        pause("speaker change",index)
                    speaker = token,tokens[index+1]
                    ox,oy = (15,9) if token in (-17,-19) else (95,119)
                    x,y,row = ox,oy,0
                    canvas.paste(0,(0,0,320,200))
                    index += 1
                elif token in (-2,-3):
                    if row==3:
                        for delta in (3,3,3,3,3,4):
                            region = canvas.crop((ox-1,oy+delta,ox+207,oy+delta+72))
                            canvas.paste(region,(ox-1,oy))
                            canvas.paste(74,(ox,oy+72,ox+208,oy+73))
                        row -= 1
                    row += 1
                    x,y = ox,oy+row*19
                    if token==-3:
                        pause("continuation -3",index,True)
                        # 16c57读键后用frame13擦箭头；这16x16不猜背景，后续字形会重建已知mask。
                        canvas.paste(0,(ox+96,oy+57,ox+112,oy+73))
                elif token==-1:
                    pause("fragment end",index)
                    if index!=len(tokens)-1:
                        raise ValueError("tokens after terminator")
                elif 0<=token<1824:
                    canvas.paste(74,(x,y,x+16,y+16))
                    if token!=10:
                        glyph = font[token*32:(token+1)*32]
                        if len(glyph)!=32:
                            raise ValueError("glyph bounds invalid")
                        for gy in range(16):
                            bits = int.from_bytes(glyph[gy*2:gy*2+2],"big")
                            for gx in range(16):
                                if bits&(0x8000>>gx):
                                    canvas.putpixel((x+gx,y+gy),205)
                                    canvas.putpixel((x+gx-1,y+gy+1),76)
                                    canvas.putpixel((x+gx,y+gy+1),76)
                    x += 16
                else:
                    raise ValueError(f"unknown text control {token}")
                index += 1
    if len(pages)!=117:
        raise ValueError("native opening wait contract changed")
    return pages


def page_match(frame,page,assets):
    text = match_indexed(frame,page["text"],(0,0))
    border = match_indexed(frame,assets["dialog_top"],page["border_position"])
    if not text["ok"] or not border["ok"]:
        return None
    portrait = next(({"frame":i*3,**found} for i,image in enumerate(page["portraits"])
                     if (found:=exact_tile(frame,image,page["portrait_position"]))),None)
    if portrait is None:
        return None
    arrow = None
    if page["arrow_position"] is not None:
        arrow = next(({"frame":18+i,**found} for i,image in enumerate(page["arrows"])
                      if (found:=exact_tile(frame,image,page["arrow_position"]))),None)
        if arrow is None:
            return None
    return {key:page[key] for key in ("stage","fragment","token_offset","reason","speaker_control",
                                     "speaker_argument","portrait_id","known_text_pixels")} | {
        "kind":"resource_proven_opening_wait","text":text,"border":border,"portrait":portrait,"arrow":arrow}


def geometry(original):
    """3231b的四次persistent构造与0/1/2/5脚本；不使用START.SAV捕获坐标。"""
    executable = original / "FD2.EXE"
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd":
        raise ValueError("opening geometry requires the verified FD2.EXE version")
    le = parse_le(data)
    objects = parse_objects(data, le)
    if [(o.relbase, o.vsize) for o in objects] != [(0x10000,0x3ef29),(0x50000,0x56b0),(0x60000,0x34d2)]:
        raise ValueError("opening LE object layout changed")
    raw = build_relbase_image(data, le, objects)
    fixups = collect_internal_offset_fixups(executable)
    field = (original / "FDFIELD.DAT").read_bytes()
    placement = archive_entry(field, 2)
    count = struct.unpack_from("<H", placement)[0]
    metadata = archive_entry(field, 1)
    if len(placement) != 2 + count * 6 or count < 12 or metadata[:3] != bytes((0,4,30)):
        raise ValueError("stage0 placement/metadata contract changed")
    positions = [list(struct.unpack_from("<HH", placement, 2+i*6)) for i in range(count)]
    positions = positions[-4:] + positions[:8]
    ids = [0,9,4,30] + [metadata[0x83+i*26+1] for i in range(8)]
    sides = [2]*4 + [metadata[0x83+i*26] for i in range(8)]
    scripts = []
    for script in (0,1,2,5):
        obj,offset,address = fixups[0x627d8+script*4]
        if obj != 3 or not 0x60000 <= address < len(raw):
            raise ValueError("opening movement script fixup changed")
        cursor = address + 1
        for _ in range(raw[address]):
            mode,actors = raw[cursor:cursor+2]
            cursor += 2
            pairs = raw[cursor:cursor+2*actors]
            cursor += 2*actors
            if len(pairs) != 2*actors:
                raise ValueError("opening movement script truncated")
            for actor,direction in zip(pairs[::2],pairs[1::2]):
                if actor >= 12 or direction >= 4:
                    raise ValueError("opening movement actor/direction invalid")
                if not mode & 128:
                    dx,dy = ((0,1),(-1,0),(0,-1),(1,0))[direction]
                    positions[actor][0] += mode*dx
                    positions[actor][1] += mode*dy
        scripts.append({"id":script,"canonical_va":hex(address),"target_offset":hex(offset),
                        "sha256":hashlib.sha256(raw[address:cursor]).hexdigest()})
    # 3231b尾32975(9)隐藏该actor，134e4全方向0，12d7b(0)聚焦主角。
    actors = [{"raw_actor":i,"unit_id":ids[i],"side":sides[i],"position":position,
               "direction":0,"hidden":i==9,"acted":False} for i,position in enumerate(positions)]
    return {"actors":actors,"first_focus_world":positions[0].copy(),"movement_scripts":scripts,
            "active_count":12,"roster_count":4,"turn":1,"stage":0,"resource":0}


def first_browse(frame, assets, leader_world):
    leader = browse_anywhere(frame, assets, leader=True)
    if not leader or any(match_indexed(frame,assets["dialog_top"],point)["ok"] for point in ((5,2),(5,112))):
        return None
    sx,sy = leader["focus_screen"]
    camera = [leader_world[0]-(sx-4)//24,leader_world[1]-(sy-4)//24]
    if not (0<=camera[0]<=11 and 0<=camera[1]<=16):
        return None
    return {**leader,"kind":"first_natural_operable_browse","leader_world":leader_world.copy(),
            "inferred_camera":camera}


def measurement(source, before, first, expected):
    if source[0]!=0 or source[META:META+3]!=bytes((1,12,0)) or source[META+9]!=4:
        raise AssertionError("natural stage/turn/active/roster contract differs")
    if int.from_bytes(source[META+10:META+14],"little")!=0:
        raise AssertionError("natural stage0 resource was not reset to0")
    actors = []
    for contract in expected["actors"]:
        index = contract["raw_actor"]
        unit = source[UNITS+index*STRIDE:UNITS+(index+1)*STRIDE]
        actor = {"raw_actor":index,"unit_id":unit[7],"side":unit[6],"position":list(unit[:2]),
                 "direction":unit[3],"hidden":bool(unit[5]&1),"acted":bool(unit[5]&0x80)}
        if actor != contract:
            raise AssertionError(f"natural actor{index} identity/position/direction/hidden/acted differs")
        actors.append(actor)
    camera = first["inferred_camera"]
    focus = expected["first_focus_world"].copy()
    focus[0] -= 1
    relative = [focus[i]-camera[i] for i in range(2)]
    if list(source[META+3:META+5])!=camera or list(source[META+5:META+7])!=focus:
        raise AssertionError("Save changed camera or did not retain the declared single LEFT measuring focus")
    if list(source[META+7:META+9])!=relative:
        raise AssertionError("Save relative focus disagrees with camera/world focus")
    if source[0x312b:-4]!=before[0x312b:-4]:
        raise AssertionError("natural active Save changed manual slots outside checksum")
    return {"actors":actors,"active_count":12,"turn":1,"stage":0,"roster_count":4,"resource":0,
            "camera":camera.copy(),"natural_first_focus_world":expected["first_focus_world"].copy(),
            "natural_first_focus_screen":first["focus_screen"].copy(),"measuring_focus_world":focus,
            "measuring_focus_relative":relative,"measuring_navigation":["LEFT"],
            "manual_slots_preserved_except_checksum":True,"roster_unknown_bytes_not_compared_to_SDL":True}


def compare_measurements(original, sdl):
    fields = ("actors","active_count","turn","stage","roster_count","resource","camera",
              "natural_first_focus_world","natural_first_focus_screen","measuring_focus_world",
              "measuring_focus_relative","measuring_navigation")
    for field in fields:
        if original[field] != sdl[field]:
            raise AssertionError(f"natural START original/SDL mismatch: {field}")
    return {"ok":True,"compared_fields":list(fields),"actor_count":12,
            "excluded":"RNG, full scene pixels/timing/audio, unknown active and raw roster bytes"}


