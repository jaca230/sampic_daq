#!/usr/bin/env python3
"""Set the ODB fields for self-triggered SAMPIC channels with FEB L2 external gating.

Dry-run is the default.  Use --apply only after reviewing the printed paths.
"""
import argparse
import sys

ROOT = "/Equipment/SAMPIC {index:02d}/Settings"


def parse_index_selection(raw: str, upper_bound: int, label: str):
    if raw.lower() in {"all", "*"}:
        return list(range(upper_bound))
    try:
        values = sorted({int(item) for item in raw.split(",") if item.strip()})
    except ValueError as exc:
        raise ValueError(f"Invalid {label} selection: {raw}") from exc
    if not values or values[0] < 0 or values[-1] >= upper_bound:
        raise ValueError(f"{label} indices must be in [0, {upper_bound - 1}]")
    return values


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--frontend-index", type=int, default=0)
    p.add_argument("--boards", default="0")
    p.add_argument("--chips", default="all")
    p.add_argument("--channels", default="all")
    p.add_argument("--apply", action="store_true", help="Write ODB (default is dry-run).")
    p.add_argument("--ext-trigger-type", default="EXT_SIG")
    p.add_argument("--signal-level", default="TTL_SIG")
    p.add_argument("--trigger-edge", default="RISING_EDGE")
    p.add_argument("--primitive-gate-length", type=int, default=10)
    p.add_argument("--latency-gate-length", type=int, default=3)
    p.add_argument("--level2-ext-gate", type=int, default=5)
    p.add_argument("--hit-time-offset-ns", type=float, default=-470.0)
    p.add_argument("--pre-window-ns", type=float, default=20.0)
    p.add_argument("--post-window-ns", type=float, default=20.0)
    args = p.parse_args()
    try:
        boards = parse_index_selection(args.boards, 4, "board")
        chips = parse_index_selection(args.chips, 4, "chip")
        channels = parse_index_selection(args.channels, 16, "channel")
    except ValueError as exc:
        p.error(str(exc))

    root = ROOT.format(index=args.frontend_index)
    writes = [
        (f"{root}/Crate/external_trigger_type", args.ext_trigger_type),
        (f"{root}/Crate/signal_level", args.signal_level),
        (f"{root}/Crate/trigger_edge", args.trigger_edge),
        (f"{root}/Crate/primitives_gate_length", args.primitive_gate_length),
        (f"{root}/Crate/latency_gate_length", args.latency_gate_length),
        (f"{root}/Crate/enable_external_trigger_counter", True),
        (f"{root}/Crate/enable_detect_ext_trigger_id", True),
        (f"{root}/Frontend Event Collector/mode", "EXTERNAL_TRIGGER"),
        (f"{root}/Frontend Event Collector/external_trigger_mode/hit_time_offset_ns", args.hit_time_offset_ns),
        (f"{root}/Frontend Event Collector/external_trigger_mode/pre_window_ns", args.pre_window_ns),
        (f"{root}/Frontend Event Collector/external_trigger_mode/post_window_ns", args.post_window_ns),
    ]
    for b in boards:
        base = f"{root}/Crate/front_end_boards/feb{b}"
        writes += [(f"{base}/global_trigger_option", "FEB_GLOBAL_TRIGGER_IS_L2"), (f"{base}/level2_trigger_build", True),
                   (f"{base}/level2_coincidence_ext_gate", True),
                   (f"{base}/level2_ext_trig_gate", args.level2_ext_gate),
                   (f"{base}/override_level2_trigger_logic", True)]
        for c in chips:
            chip = f"{base}/sampics/sampic{c}"
            writes.append((f"{chip}/trigger_option", "SAMPIC_TRISSER_IS_FEB_GT"))
            for ch in channels:
                channel = f"{chip}/channels/channel{ch}"
                writes += [(f"{channel}/trigger_mode", "SAMPIC_CHANNEL_SELF_TRIGGER_MODE"),
                           (f"{channel}/enable_for_central_trigger", True)]
    if not args.apply:
        for path, value in writes: print(f"[DRY-RUN] {path} <- {value!r}")
        print("No ODB fields changed. Re-run with --apply to write these values.")
        return 0
    import midas.client
    client = midas.client.MidasClient("set_l2_external_trigger_mode")
    try:
        for path, value in writes:
            client.odb_set(path, value)
            print(f"{path} <- {value!r}")
    finally:
        client.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
