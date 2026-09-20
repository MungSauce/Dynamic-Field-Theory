from __future__ import annotations

from .qstate import QFormation


def settle(state: QFormation) -> QFormation:
    """Encoding-direction primitive reflex."""
    state = QFormation(state)
    if state is QFormation.NEG:
        return QFormation.PRIMED_ZERO
    if state is QFormation.POS:
        return QFormation.DECAY_ZERO
    raise ValueError("only polarized Q formations can settle")


def reverse_settle(state: QFormation) -> QFormation:
    """Reverse primitive reflex."""
    state = QFormation(state)
    if state is QFormation.PRIMED_ZERO:
        return QFormation.NEG
    if state is QFormation.DECAY_ZERO:
        return QFormation.POS
    raise ValueError("only directed-zero Q formations reverse-settle")


def is_directed_zero(state: QFormation) -> bool:
    state = QFormation(state)
    return state in (QFormation.PRIMED_ZERO, QFormation.DECAY_ZERO)
