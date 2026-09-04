#!/usr/bin/env python3
"""Summarize the CSV stream emitted by examples/08_trace."""

from __future__ import annotations

import argparse
import csv
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, TextIO


HEADER = (
    "sequence",
    "tick",
    "event",
    "task",
    "related_task",
    "object",
    "value",
)


class TraceFormatError(ValueError):
    """Raised when a trace stream cannot be decoded."""


@dataclass(frozen=True)
class Event:
    sequence: int
    tick: int
    event: str
    task: str
    related_task: str
    object_name: str
    value: int


def _parse_integer(value: str, field: str, line_number: int) -> int:
    try:
        return int(value, 10)
    except ValueError as error:
        raise TraceFormatError(
            f"line {line_number}: {field} is not a decimal integer: {value!r}"
        ) from error


def _parse_event(row: list[str], line_number: int) -> Event:
    if len(row) != len(HEADER):
        raise TraceFormatError(
            f"line {line_number}: expected {len(HEADER)} columns, got {len(row)}"
        )
    if not row[2]:
        raise TraceFormatError(f"line {line_number}: event name is empty")
    return Event(
        sequence=_parse_integer(row[0], "sequence", line_number),
        tick=_parse_integer(row[1], "tick", line_number),
        event=row[2],
        task=row[3] or "-",
        related_task=row[4] or "-",
        object_name=row[5] or "-",
        value=_parse_integer(row[6], "value", line_number),
    )


def read_trace(stream: TextIO) -> tuple[list[Event], int]:
    events: list[Event] = []
    dropped: int | None = None
    header_seen = False

    for line_number, raw_line in enumerate(stream, start=1):
        line = raw_line.rstrip("\r\n")
        if not line.strip():
            continue
        if line.startswith("#"):
            if not line.startswith("# dropped="):
                raise TraceFormatError(f"line {line_number}: unknown metadata")
            if dropped is not None:
                raise TraceFormatError(f"line {line_number}: duplicate dropped metadata")
            dropped = _parse_integer(line[len("# dropped=") :], "dropped", line_number)
            if dropped < 0:
                raise TraceFormatError(f"line {line_number}: dropped is negative")
            continue

        row = next(csv.reader([line]))
        if not header_seen:
            if tuple(row) != HEADER:
                raise TraceFormatError(
                    f"line {line_number}: invalid header; expected {','.join(HEADER)}"
                )
            header_seen = True
            continue
        events.append(_parse_event(row, line_number))

    if not header_seen:
        raise TraceFormatError("trace does not contain a header")
    if dropped is None:
        raise TraceFormatError("trace does not contain '# dropped=' metadata")
    return events, dropped


def _task_name(name: str) -> str:
    return name if name else "-"


def render_report(events: Iterable[Event], dropped: int) -> str:
    event_list = list(events)
    counts = Counter(event.event for event in event_list)
    switch_counts = Counter(
        _task_name(event.task)
        for event in event_list
        if event.event == "task-switch"
    )
    first_tick: dict[str, int] = {}
    last_tick: dict[str, int] = {}
    for event in event_list:
        task = _task_name(event.task)
        if task == "-":
            continue
        first_tick[task] = min(first_tick.get(task, event.tick), event.tick)
        last_tick[task] = max(last_tick.get(task, event.tick), event.tick)

    lines = [
        "Trace summary",
        f"events: {len(event_list)}",
        f"dropped: {dropped}",
        "event counts:",
    ]
    lines.extend(f"  {name}: {counts[name]}" for name in sorted(counts))
    lines.append("task switches:")
    for task in sorted(switch_counts):
        lines.append(
            f"  {task}: {switch_counts[task]} "
            f"(ticks {first_tick[task]}..{last_tick[task]})"
        )
    lines.append("timeline:")
    lines.extend(
        f"  {event.tick} {_task_name(event.task)} {event.event} "
        f"{event.object_name} {event.value}"
        for event in event_list
    )
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "trace",
        nargs="?",
        default="-",
        help="CSV trace path, or '-' for stdin (default)",
    )
    args = parser.parse_args(argv)

    stream: TextIO
    close_stream = False
    if args.trace == "-":
        stream = sys.stdin
    else:
        try:
            stream = Path(args.trace).open("r", encoding="utf-8", newline="")
        except OSError as error:
            print(f"trace_report: {error}", file=sys.stderr)
            return 2
        close_stream = True

    try:
        events, dropped = read_trace(stream)
    except (OSError, csv.Error, TraceFormatError) as error:
        print(f"trace_report: {error}", file=sys.stderr)
        return 2
    finally:
        if close_stream:
            stream.close()

    print(render_report(events, dropped))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
