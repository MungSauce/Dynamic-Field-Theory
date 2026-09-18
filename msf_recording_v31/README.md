# MSF Recording v3.1 — packed literal signal

This revision preserves the v3 orchestra/recording/audience model. The only change is how the literal composite signal samples are stored.

Two adjacent composite signal samples are combined into one 31-bit signal code and the codes are bit-packed. The packer sees signal amplitudes only; it has no access to pages, source symbols, instrument states, or note identities.

Pipeline:
source -> orchestra -> composite signal -> signal-sample packer -> .msf -> signal-sample unpacker -> audience -> source

The primary compression score is signal payload bytes / source bytes. Container metadata is reported separately.
