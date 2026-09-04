#!/usr/bin/env python3

import io
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.trace_report import TraceFormatError, read_trace, render_report


VALID_TRACE = """sequence,tick,event,task,related_task,object,value
0,0,task-switch,"TIMER","-","-",7
1,2,timer-callback,"TIMER","-","0x1a","1"
# dropped=0
"""


class TraceReportTests(unittest.TestCase):
    def test_read_and_render_valid_trace(self):
        events, dropped = read_trace(io.StringIO(VALID_TRACE))

        self.assertEqual(dropped, 0)
        self.assertEqual(len(events), 2)
        self.assertEqual(events[1].event, "timer-callback")
        report = render_report(events, dropped)
        self.assertIn("events: 2", report)
        self.assertIn("timer-callback: 1", report)
        self.assertIn("TIMER: 1 (ticks 0..2)", report)

    def test_rejects_bad_header(self):
        with self.assertRaises(TraceFormatError):
            read_trace(io.StringIO("not-a-header\n# dropped=0\n"))

    def test_rejects_bad_columns(self):
        source = (
            "sequence,tick,event,task,related_task,object,value\n"
            "0,0,task-switch,task,-,-\n"
            "# dropped=0\n"
        )
        with self.assertRaises(TraceFormatError):
            read_trace(io.StringIO(source))

    def test_requires_dropped_metadata(self):
        source = "sequence,tick,event,task,related_task,object,value\n"
        with self.assertRaises(TraceFormatError):
            read_trace(io.StringIO(source))


if __name__ == "__main__":
    unittest.main()
