# Pimega area detector IOC

This is an IOC for all the Pimega area detector variations (540D and 135D). It must be included in compiled in the areaDetector folder of synApps.

## dependencies
* [pimega-api](https://gitlab.cnpem.br/DET/pimega/pimega-api.git)

## build
make sure your EPICS_HOST_ARCH environment variable is set. Possible values are linux-ppc64 or linux-x86_64. After that run the following

```
cd ADPimega
make -j4
cd iocs/pimegaIOC/iocBoot/iocPimega
make envPaths
```

## Run
```
cd iocs/pimegaIOC/iocBoot/iocPimega
./<Choose detector command file>.cmd
```

## Test
To execute tests, first you need the requirements listed on `requirements-dev.txt`. Create a Python virtual environment and install those requirements.

When installed, just call `pytest` with the virtual environment active.
To enable verbose output, call `pytest -s`.

```bash
$ python3 -m venv venv
$ source venv/bin/activate
$ pip3 install -r requirements-dev.txt
$ pytest -s .  # -> Run this to start the test
```

__Important__: use the file `test/pytest.ini` to set custom EPICS and pytest configuration, if necessary.
