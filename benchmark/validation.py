"""Correctness checks shared by legacy benchmark runners."""


def validate_trial_output(expected, actual, pipeline, trial):
    if expected is None:
        return actual
    if actual != expected:
        raise RuntimeError(
            f"output mismatch for {pipeline} trial {trial}: "
            f"{actual} != {expected}")
    return expected
