"""Configuration file for AD Pimega tests."""
import os

import pytest
from pytest import ExitCode


def pytest_addoption(parser):
    """Pytest command line arguments."""
    parser.addini("epics_prefix", help="IP Address to connect")


def pytest_configure(config):
    """Allow plugins and conftest files to perform initial configuration.
       Configure the Pytest environment before test starts."""
    epics_addr_list = os.getenv("EPICS_CA_ADDR_LIST")
    if not epics_addr_list:
        print("EPICS_CA_ADDR_LIST not found. May be missing from .bashrc")
        # pytest.exit(ExitCode.INTERNAL_ERROR)

    print(f"EPICS_CA_ADDR_LIST: {epics_addr_list}")


def pytest_unconfigure(config):
    """Called before test process is exited."""


@pytest.fixture(scope="session")
def prefix(request):
    """Returns the EPICS prefix to access an equipment.
    This value is set on pytest.ini.
    """
    return request.config.getini("epics_prefix")
