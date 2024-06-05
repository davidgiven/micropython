#!/usr/bin/env python


from collections import defaultdict
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../tools"))
import boardgen


# These are the columns of the af.csv as well as the arguments to the PIN()
# macro in pins_prefix.c.
AFS = {
    "TLSR8232": ["af0", "af1", "af2", "af3", "af4"],
}


class Tc32Pin(boardgen.Pin):
    def __init__(self, cpu_pin_name):
        super().__init__(cpu_pin_name)

        # P<port><num> (already verified by validate_cpu_pin_name).
        self._port = cpu_pin_name[1]
        self._pin = cpu_pin_name[2]

        self._afs = defaultdict(lambda: "GPIO")

    # Called for each AF defined in the csv file for this pin.
    def add_af(self, af_idx, af_name, af):
        name = AFS[self._generator.args.mcu][af_idx]
        assert name == af_name.lower()
        v = af
        self._afs[AFS[self._generator.args.mcu][af_idx]] = v

    # Use the PIN() macro defined in pins_prefix.c for defining the pin
    # objects.
    def definition(self):
        # TLSR8232: PIN(p_name, p_pin)
        return "PIN({:s}, {})".format(
            self.name(),
            ", ".join(
                "{}".format(self._afs[x]) for x in AFS[self._generator.args.mcu]
            ),
        )

    @staticmethod
    def validate_cpu_pin_name(cpu_pin_name):
        boardgen.Pin.validate_cpu_pin_name(cpu_pin_name)


class Tc32PinGenerator(boardgen.PinGenerator):
    def __init__(self):
        # Use custom pin type above, and also enable the --af-csv argument so
        # that add_af gets called on each pin.
        super().__init__(
            pin_type=Tc32Pin,
            enable_af=True,
        )

    # Override the default implementation just to change the default arguments
    # (extra header row, skip first column).
    def parse_af_csv(self, filename):
        return super().parse_af_csv(
            filename, header_rows=1, pin_col=0, af_col=1
        )

    # We need to know the mcu to emit the correct AF list.
    def extra_args(self, parser):
        parser.add_argument("--mcu")


if __name__ == "__main__":
    Tc32PinGenerator().main()
