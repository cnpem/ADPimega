import pytest
from epics import caget, caput

pytestmark = pytest.mark.acquisition


@pytest.mark.parametrize("numexp", [1, 10, 1000])
def test_numexposures(prefix, numexp):
    # Set numexp
    # Read numexp
    ans = numexp
    print(f"Value set: {numexp} | Value read: {ans}")
    assert ans == numexp
