from qomputation.dependencies import MODULE_DEPS, assert_acyclic, dependency_closure


def test_graph_is_acyclic_and_closed():
    assert_acyclic()
    for module, deps in MODULE_DEPS.items():
        for dep in deps:
            assert dep in MODULE_DEPS


def test_qompress_closure_contains_native_base():
    closure = dependency_closure("qompress")
    for required in ("qstate", "qnode", "qfield", "qregister", "qseed", "qompress"):
        assert required in closure
