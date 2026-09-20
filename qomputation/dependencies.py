from __future__ import annotations

MODULE_DEPS = {
    "qstate": (),
    "qreflex": ("qstate",),
    "qinteger": ("qstate", "qreflex"),

    "qnode": ("qstate", "qinteger"),
    "qaddress": ("qnode",),
    "qalloc": ("qnode", "qaddress"),

    "qfield": ("qstate", "qnode", "qaddress", "qinteger"),
    "qregister": ("qfield", "qinteger"),
    "qtopology": ("qnode", "qalloc", "qfield", "qregister"),

    "qisa": ("qstate", "qnode", "qaddress", "qregister"),
    "qcontrol": ("qisa", "qfield"),
    "qexec": ("qisa", "qcontrol", "qtopology"),
    "qstack": ("qexec", "qregister"),

    "qmem": ("qtopology", "qregister"),
    "qio": ("qexec", "qmem"),
    "qclock": ("qexec",),
    "qfault": ("qexec",),

    "qbin": ("qisa", "qtopology"),
    "qloader": ("qbin", "qmem"),
    "qkernel": ("qloader", "qexec", "qio", "qclock", "qfault"),
    "qabi": ("qkernel", "qstack"),

    "qasm": ("qisa", "qabi"),
    "qlang": ("qasm",),
    "qcompiler": ("qlang", "qasm"),
    "qlink": ("qcompiler", "qbin"),
    "qdebug": ("qexec", "qclock", "qfault"),

    "qbutton": ("qstate", "qregister"),
    "qgrammar": ("qbutton", "qcontrol"),
    "qseed": ("qtopology", "qregister"),
    "qompress": ("qgrammar", "qseed", "qfield"),

    "qapp": ("qabi", "qbin"),
    "qui": ("qapp", "qio"),
    "qfs": ("qkernel", "qseed"),
    "qpkg": ("qapp", "qfs", "qlink"),
}


def dependency_closure(module: str) -> tuple[str, ...]:
    if module not in MODULE_DEPS:
        raise KeyError(module)
    out: list[str] = []
    seen: set[str] = set()

    def visit(name: str) -> None:
        for dep in MODULE_DEPS[name]:
            visit(dep)
        if name not in seen:
            seen.add(name)
            out.append(name)

    visit(module)
    return tuple(out)


def assert_acyclic() -> None:
    visiting: set[str] = set()
    done: set[str] = set()

    def visit(name: str) -> None:
        if name in visiting:
            raise AssertionError(f"dependency cycle at {name}")
        if name in done:
            return
        visiting.add(name)
        for dep in MODULE_DEPS[name]:
            if dep not in MODULE_DEPS:
                raise AssertionError(f"{name} depends on undefined {dep}")
            visit(dep)
        visiting.remove(name)
        done.add(name)

    for name in MODULE_DEPS:
        visit(name)
