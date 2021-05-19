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
