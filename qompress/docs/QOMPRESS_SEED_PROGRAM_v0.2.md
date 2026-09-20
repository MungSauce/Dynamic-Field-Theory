# Qompress Seed Program v0.2

The decoder/machine is fixed and blank. The seed is the complete corpus-specific path program.

Each page stores only nonzero XOR changes from the preceding relational field. The first page is a transition from all-primed-zero.

Per page, the encoder chooses the smaller of:

- sparse delta-coded changed positions plus XOR bytes;
- dense change bitmap plus XOR bytes.

The reference compiler tests several page widths and keeps the smallest complete seed. Future COPY/RUN/context/topology relations are allowed only if every corpus-specific selector/model byte remains counted.
