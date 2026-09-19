# Q-OS boot topology. No IF/THEN control flow: geometry + strikes + settlement.
MATRIX BOOT_CONFORMANCE
NODE 0 -+
NODE 1 +-
NODE 2 -+
EDGE 0 1 CASCADE
EDGE 1 2 CANCEL
STRIKE 0 +
SETTLE 128
EXPECT 0 +
EXPECT 1 -+
EXPECT 2 +-
END
