"""版本 3 冷表的具名字段与原字节／C 数组之间的可逆投影。"""
from __future__ import annotations

import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
TABLE_SCHEMA_VERSION = 3
INTEGER_TYPES = {
    "uint8_t": (0, 255), "int8_t": (-128, 127),
    "uint16_t": (0, 65535), "int16_t": (-32768, 32767),
    "uint32_t": (0, 4294967295), "int32_t": (-2147483648, 2147483647),
    "int": (-2147483648, 2147483647),
}
STRUCT_TYPES = {"fd2_field_stage_music": ("uint8_t", "uint8_t")}


def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def decode_json(raw):
    return json.loads(raw.decode("utf-8"), object_pairs_hook=reject_duplicates,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def load_json(path: Path):
    return decode_json(path.read_bytes())


def integer(value, c_type):
    lo, hi = INTEGER_TYPES[c_type]
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError(f"{c_type} value outside [{lo}, {hi}]: {value!r}")


def validate_values(values, dimensions, c_type):
    if dimensions:
        if not isinstance(values, list) or len(values) != dimensions[0]:
            raise ValueError(f"array extent mismatch: expected {dimensions}")
        for value in values:
            validate_values(value, dimensions[1:], c_type)
    elif c_type in STRUCT_TYPES:
        fields = STRUCT_TYPES[c_type]
        if not isinstance(values, list) or len(values) != len(fields):
            raise ValueError(f"{c_type}: wrong field count")
        for value, field in zip(values, fields):
            integer(value, field)
    else:
        integer(values, c_type)


def pack_record(values, formats):
    packed = []
    for value, fmt in zip(values, formats):
        if fmt.endswith("s"):
            validate_values(value, [int(fmt[:-1])], "uint8_t")
            packed.append(bytes(value))
        elif type(value) is int:
            packed.append(value)
        else:
            raise ValueError(f"expected integer, got {value!r}")
    try:
        return list(struct.pack("<" + "".join(formats), *packed))
    except struct.error as error:
        raise ValueError(str(error)) from error


class TableLayout:
    """同一组字段路径用于读取 Mod 和导出原数据。"""

    def __init__(self, contracts, *, document=None, values=None):
        self.contracts = contracts
        self.input = document
        self.reading = values is None
        self.values = {} if self.reading else values
        self.document = {"schema_version": TABLE_SCHEMA_VERSION}
        self.bound = set()

    def field(self, path, value=None):
        if self.reading:
            try:
                value = self.input
                for key in path:
                    value = value[key]
            except (KeyError, IndexError, TypeError) as error:
                raise ValueError(f"missing field or invalid shape at {'.'.join(map(str, path))}") from error
        parent = self.document
        for key, next_key in zip(path, path[1:]):
            if isinstance(key, int):
                while len(parent) <= key:
                    parent.append([] if isinstance(next_key, int) else {})
            else:
                parent.setdefault(key, [] if isinstance(next_key, int) else {})
            parent = parent[key]
        key = path[-1]
        if isinstance(key, int):
            while len(parent) <= key:
                parent.append(None)
        parent[key] = value
        return value

    def bind(self, symbol, paths, packed=None):
        def transfer(paths, value=None):
            if isinstance(paths, tuple):
                return self.field(paths, value)
            if self.reading:
                return [transfer(path) for path in paths]
            return [transfer(path, item) for path, item in zip(paths, value)]

        self.bound.add(symbol)
        if self.reading:
            values = transfer(paths)
            if packed:
                values = [pack_record(row, packed) for row in values]
            self.values[symbol] = values
        else:
            values = self.values[symbol]
            if packed:
                row_format = struct.Struct("<" + "".join(packed))
                values = [[list(v) if isinstance(v, bytes) else v
                           for v in row_format.unpack(bytes(row))] for row in values]
            transfer(paths, values)

    def extent(self, symbol):
        return self.contracts[symbol]["dimensions"][0]

    def plain(self, **symbols):
        for key, symbol in symbols.items():
            self.bind(symbol, (key,))

    def column(self, symbol, key, *fields, first_id=None):
        identities = range(self.extent(symbol))
        self.bind(symbol, [(key, i if first_id is None else str(i + first_id), *fields)
                           for i in identities])

    def rows(self, symbol, key, fields, *, first_id=None, prefix=(), packed=None):
        identities = range(self.extent(symbol))
        self.bind(symbol, [[(key, i if first_id is None else str(i + first_id), *prefix, field)
                            for field in fields.split()] for i in identities], packed)

    def record(self, symbol, key, fields):
        self.bind(symbol, [(key, field) for field in fields.split()])


def items(t):
    t.rows("ITEM_RECORDS", "items", "category attack_bonus hit_bonus defense_bonus evasion_bonus "
           "weapon_effect weapon_effect_parameter attack_min_range attack_max_range use_effect "
           "use_parameter use_range ai_target_filter effect_range buy_price target_filter unknown_16",
           first_id=0, packed="B h h h h B B B B B H B B B H B B".split())
    t.rows("profiles", "equipment_profiles", "allowed_categories unknown_06", packed=("6s", "B"))


def magic(t):
    t.rows("MAGIC_RECORDS", "spells", "effect_value accuracy cast_range effect_range mp_cost target_filter",
           first_id=0, packed="HBBBBB")
    t.column("MAGIC_MAP_EFFECT_IDS", "spells", "map", "effect_id", first_id=0)
    t.column("MAGIC_MAP_COST_IDS", "spells", "map", "cost_record_id", first_id=0)
    t.column("MAGIC_MAP_TARGET_LIMITS", "spells", "map", "target_limit", first_id=0)
    t.column("MAGIC_MAP_COST_BEFORE_CAST", "spells", "map", "cost_before_cast", first_id=0)
    t.plain(damage_scale_by_profile="MAGIC_DAMAGE_PROFILE_SCALES")


def unit_base(t):
    t.rows("k_player_base_records", "players", "race movement_profile initial_level base_hp base_mp "
           "movement_points initial_magic_mask equipment inventory base_attack base_defense base_accuracy",
           first_id=0, packed="B B B H H B I 2s 4s H H H".split())
    t.rows("k_enemy_records", "enemies", "race movement_profile hp_per_level mp_per_level attack_per_level "
           "defense_per_level accuracy_per_level movement_points experience_coefficient",
           first_id=68, packed="BBHBBBBBB")
    t.rows("k_growth_records", "growth", "attack_per_level attack_on_level_up defense_per_level "
           "defense_on_level_up accuracy_per_level accuracy_on_level_up hp_per_level hp_on_level_up "
           "mp_per_level mp_on_level_up learning_id", first_id=0)
    t.bind("k_learning_records", [[("learning", i, j, field)
                                   for j in range(6) for field in ("level", "magic_id")]
                                  for i in range(t.extent("k_learning_records"))])
    t.rows("k_player_base_alias_views", "constructor_aliases", "unit_id source_kind byte_offset")


def stage(t):
    t.rows("k_stage_music", "stages", "primary alternate", first_id=0, prefix=("music",))
    t.rows("k_stage_prepare", "stages", "town_type hidden_shop_focus hidden_shop_scan_code "
           "weapon_stock item_stock hidden_stock", first_id=1, prefix=("preparation",),
           packed=("B", "B", "B", "12s", "8s", "8s"))


def attack(t):
    t.column("k_terrain_attack_percent", "terrain", "attack_percent")
    t.column("k_terrain_defense_percent", "terrain", "defense_percent")
    t.plain(critical_thresholds="k_critical_by_profile")


def preparation(t):
    t.plain(no_town_by_stage="k_prepare_no_town", promotion_item_by_unit="k_prepare_special_items",
            notice_origins_by_portrait="k_prepare_notice_portrait_origins")
    t.rows("k_prepare_promotion_records", "promotions", "movement_profile movement_bonus", first_id=32)
    t.rows("k_prepare_deployment_roster_gate", "deployment_roster_rules",
           "first_stage last_stage active_slot required_text_id")
    t.column("k_prepare_town_background_entries", "towns", "background_entry")
    for axis in ("x", "y"):
        t.bind(f"k_prepare_town_{axis}", [[("towns", i, "service_positions", j, axis)
                                         for j in range(6)] for i in range(3)])
    t.column("k_prepare_town_portraits", "services", "portrait_id")
    t.column("k_prepare_town_service_entries", "services", "fdother_entry")
    for kind, count in (("buy", 4), ("sell", 2)):
        t.bind(f"k_prepare_{kind}_messages", [[("services", i, f"{kind}_fragments", j)
                                              for i in range(6)] for j in range(count)])
    t.column("k_prepare_service_return_prefix", "services", "return_fragment")
    # 原返回提示的第六项与复活费用的索引 0 共址，只编辑一份。
    t.bind("k_prepare_revive_coefficients", [("services", 5, "return_fragment")] +
           [("revive_coefficient_by_profile", i) for i in range(28)])
    t.column("k_prepare_menu_slide_offsets", "menu_offsets", "slide")
    t.column("k_prepare_menu_cursor_offsets", "menu_offsets", "cursor")


def battle_animation(t):
    t.record("k_battle_summary_bottom_override", "summary_bottom_override", "stage raw_unit_index")
    t.rows("k_battle_ai_ray_item_present", "ai_ray_item_presentation", "operation value step")
    t.rows("k_battle_map_cast_preamble", "map_cast_preamble", "use_magic_rgb wait_ticks")
    t.rows("k_battle_cast_colors", "cast_colors", "red green blue")
    for axis in ("x", "y"):
        t.column(f"shake_{axis}", "physical_shake", axis)
        t.column(f"k_battle_magic_shake_{axis}", "magic_shake", axis)
    for symbol, field in (("start", "first_frame"), ("count", "frame_count"), ("cue", "initial_sample")):
        t.column(f"k_battle_map_fx_{symbol}", "map_fx", field)
    t.column("k_battle_map_damage_tint", "map_fx", "damage_tint")
    for field in ("red", "green", "blue", "banks"):
        t.column(f"k_battle_composite_{field}", "composite_fx", "sfx_bank" if field == "banks" else field,
                 first_id=32)
    t.rows("k_battle_map_extra_cues", "map_extra_cues", "core_id frame sample")
    t.rows("k_battle_heal_masks", "heal_masks", "initial_radius radius_step", first_id=13)
    t.rows("k_battle_item_presentations", "item_presentations", "actor_style popup_kind numeric_zero_on_success",
           first_id=5)
    t.plain(healing_curve="k_battle_heal_curve", miss_glyphs="k_battle_popup_miss",
            default_scene_by_stage="k_battle_default_scene")
    t.bind("k_battle_map_solid_color", [("map_solid_color",)])
    t.record("k_battle_map_action_parameters", "map_action",
             "short_sfx_bank recovery_sfx_bank short_sprite_stride panel_first_frame panel_frame_count "
             "panel_delay_ms effect_delay_ms bar_delay_ms counter_top_bar_width")
    short_fx(t)


def short_fx(t):
    offsets_name, stream_name = "k_battle_short_fx_offsets", "k_battle_short_fx_sequences"
    sequence_path, category_path = ("short_fx_sequences",), ("short_fx_sequence_by_category",)
    t.bound.update((offsets_name, stream_name))
    if t.reading:
        sequences, references = t.field(sequence_path), t.field(category_path)
        if not isinstance(sequences, list) or len(sequences) != 6:
            raise ValueError("short_fx_sequences requires six frame sequences")
        offsets, stream = [], []
        for sequence in sequences:
            if not isinstance(sequence, list) or not sequence:
                raise ValueError("short FX sequence requires at least one frame")
            offsets.append(len(stream))
            stream.append(len(sequence))
            for frame in sequence:
                if not isinstance(frame, dict) or set(frame) != {"frame", "sample"}:
                    raise ValueError("short FX frame requires frame and sample")
                stream.extend([frame["frame"], frame["sample"]])
        validate_values(references, [t.extent(offsets_name)], "uint8_t")
        if any(index >= len(offsets) for index in references):
            raise ValueError("invalid short FX sequence reference")
        t.values.update({offsets_name: [offsets[i] for i in references], stream_name: stream})
    else:
        stream = t.values[stream_name]
        sequences, offsets = [], []
        at = 0
        while at < len(stream):
            offsets.append(at)
            end = at + 1 + 2 * stream[at]
            sequences.append([{"frame": stream[i], "sample": stream[i + 1]} for i in range(at + 1, end, 2)])
            at = end
        t.field(sequence_path, sequences)
        t.field(category_path, [offsets.index(offset) for offset in t.values[offsets_name]])


def magic_fx(t):
    for field, suffix in (("friendly_clip", "friendly_clips"), ("hostile_clip", "hostile_clips"),
                          ("sfx_bank", "banks"), ("hp_denominator", "hp_denominators"),
                          ("minimum_sample_count", "sample_counts"), ("minimum_part_count", "minimum_parts"),
                          ("hit_frame_base", "hit_frame_bases"), ("return_lut_base", "return_lut_bases")):
        t.column(f"k_fx_{suffix}", "effects", field, first_id=0)
    for key, fields in (("rain0", ("x", "y", "front")), ("rain1", ("x", "y")),
                        ("shower", ("x", "y", "parts")), ("orbit", ("dx", "dy"))):
        for field in fields:
            t.column(f"k_fx_{key}_{field}", key, field)
    t.plain(dust_x="k_fx_dust_x", surge_x="k_fx_surge_x", burst_offsets="k_fx_burst_offsets")


def common_event(t):
    t.rows("k_common_event_nested_descriptors", "nested_drops", "mode parameter", packed="BH")
    t.plain(cell_items="k_common_event_cell_items")


def field_audio(t):
    t.rows("k_sfx_cues", "cues", "bank sample")
    t.plain(footstep_classes="k_footstep_classes")


def field_background(t):
    t.rows("k_field_background_config", "stages", "kind fdother_entry width height", first_id=0)
    t.plain(wave_offsets="k_field_background_wave")
    t.bind("k_field_background_scroll_initial", [("initial_scroll_speed",)])


def field_game_effects(t):
    t.rows("k_field_quake_parameters", "earthquake", "x_offset_fp y_offset_fp step_fp")


def field_info(t):
    t.record("k_field_overview_condition_policy", "conditions", "fragment_base stage_stride defeat_offset")
    t.rows("k_field_overview_roster_rules", "roster_rules", "stage text_id absent_delta present_delta")


def movement(t):
    t.plain(cost_profiles="k_profiles")


def runtime(t):
    t.plain(range_frames="k_field_range_sequence", auxiliary_color_by_side="k_field_auxiliary_side_colors")


LAYOUTS = {layout.__name__: layout for layout in (attack, battle_animation, common_event, field_audio,
    field_background, field_game_effects, field_info, items, magic, magic_fx, movement, preparation,
    runtime, stage, unit_base)}


def table_contracts():
    return load_json(ROOT / "data/static/manifest.json")["table_contracts"]


def project(stem, contracts, **data):
    if stem not in LAYOUTS or not isinstance(contracts, dict):
        raise ValueError(f"{stem}: unknown table layout")
    for symbol, shape in contracts.items():
        if (not isinstance(shape, dict) or set(shape) != {"c_type", "dimensions"} or
                shape["c_type"] not in INTEGER_TYPES.keys() | STRUCT_TYPES.keys() or
                not isinstance(shape["dimensions"], list) or not 1 <= len(shape["dimensions"]) <= 3 or
                any(type(n) is not int or not 1 <= n <= 65536 for n in shape["dimensions"])):
            raise ValueError(f"{stem}/{symbol}: invalid table contract")
    layout = TableLayout(contracts, **data)
    try:
        LAYOUTS[stem](layout)
    except (KeyError, IndexError, struct.error) as error:
        raise ValueError(f"{stem}: incomplete table contract or source values: {error}") from error
    if layout.bound != contracts.keys() or layout.values.keys() != contracts.keys():
        raise ValueError(f"{stem}: table contract coverage differs from layout")
    return layout


def project_tables(stem, document, contracts=None):
    if (not isinstance(document, dict) or type(document.get("schema_version")) is not int or
            document["schema_version"] != TABLE_SCHEMA_VERSION):
        raise ValueError(f"{stem}: expected table schema_version {TABLE_SCHEMA_VERSION}")
    if contracts is None:
        contracts = table_contracts()[stem + ".json"]
    try:
        layout = project(stem, contracts, document=document)
        if document != layout.document:
            raise ValueError("unknown field or invalid collection extent")
        for symbol, shape in contracts.items():
            validate_values(layout.values[symbol], shape["dimensions"], shape["c_type"])
    except ValueError as error:
        raise ValueError(f"{stem}: {error}") from error
    return [{"name": symbol, **shape, "values": layout.values[symbol]} for symbol, shape in contracts.items()]


def table_definition(stem, values, contracts=None):
    if contracts is None:
        contracts = table_contracts()[stem + ".json"]
    for symbol, shape in contracts.items():
        validate_values(values[symbol], shape["dimensions"], shape["c_type"])
    document = project(stem, contracts, values=values).document
    # 一处绑定错位会破坏导出，写文件前校验能否无损读回。
    if {row["name"]: row["values"] for row in project_tables(stem, document, contracts)} != values:
        raise ValueError(f"{stem}: named definition loses source values")
    return document


def load_table_document(path, stem=None):
    return {"schema_version": TABLE_SCHEMA_VERSION,
            "tables": project_tables(stem or path.stem, load_json(path))}
