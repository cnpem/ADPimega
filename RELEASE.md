# ADPimega Releases

The release notes follow the core concepts proposed by [Common
ChangeLog](https://common-changelog.org) with minor tweaks, namely the use of
"Unreleased" header, lack of references for each entry and lack of date on
released versions.

## Unreleased

### Changed

- Defer stopping the backend to `captureTask` (Henrique F. Simoes)
  - Ensure the `captureTask` is always consistent with the backend state by
    avoiding interactions directly to the backend in the `acqTask`. This
    prevents deadlocks in the IOC when the `captureTask` and the backend state
    diverge.
- Don't accept to capture zero images (Henrique F. Simoes)
  - Prevent this invalid number of images to be sent to the backend and leave
    it in an invalid state.

### Fixed

- Improve error handling when starting an acquisition (Henrique F. Simoes)
  - When an improbable error occurs when setting the number of images to be
    collected, correctly report that the acquisition failed.
- Use `libpimega` definition for maximum filename length (Henrique F. Simoes)
  - Remove the hard limit of 300 bytes defined by ADPimega itself and use the
    library supported limit instead.

## 2.6.0

Users interested in changing pixel modes, diagnostic PVs for PIMEGA 450D(S) or
a better reporting of the current number of acquired images should upgrade to
this release.

### Changed

- **Breaking**: Use global acquisition buffer usage status (Henrique F. Simoes)
  - Remove the module-specific `M<n>:Backend_BufferUsed_RBV` for all detector
    module `<n>`. They wouldn't contain data except for the first one, whose
    value would be redundant with the general `Backend_BufferUsed_RBV` PV.
  - The general PV `Backend_BufferUsed_RBV` is kept and should be used to get
    the buffer usage statistics.
- Make `PixelMode` PV properly change between pixel modes (Álvaro Costa)
  - Use `PixelMode()` API to properly change the chip counter when switching
    between Charge Summing Mode (CSM) and Single Pixel Mode (SPM).
- Make `NumImagesCounter_RBV` be the minimum of the images acquired by each
  detector module (Álvaro Costa)
  - Use the new acquisition status API from libpimega which properly exposes
    this information
- Refactor iocsh start-up scripts (Henrique F. Simoes)
  - Users are expected to load directly the database file for the detector
    model in use (e.g. `pimega540d.db`) to get all PVs. This way, the correct
    number of PVs is created for each detector module and the general PVs are
    also loaded.
  - Example start-up scripts load reusable iocsh templates, which receive
    arguments to modify their settings. Users should be able to use these
    template files as-is, without requiring any modification, and get the basic
    instantiation of the IOC and plugins.
  - Halt on any non-interactive iocsh error
  - Clean up stale autosave-related settings

### Added

- Support diagnostic PVs for all detector modules (Henrique F. Simoes)
  - The correct number of PVs is exported through auto-generated templates.
    This is specially relevant for PIMEGA 450D and 450DS, which have more than
    4 modules. PIMEGA 135D also has fewer unused PVs instantiated with this
    feature, since their template would previously contain hard-coded PVs for
    three non-existent modules.
  - This includes the following PVs for a module `<n>`:
    - `M<n>:MB_Temperature_RBV`
    - `M<n>:MB_AvgTemperature_RBV`
    - `M<n>:Sensor_Temperature_RBV`
    - `M<n>:Medipix_AvgTemperature_RBV`
    - `M<n>:Temperature_Status`
    - `M<n>:Highest_Temperature`
    - `M<n>:LostFrameCount_RBV`
    - `M<n>:RxFrameCount_RBV`
    - `M<n>:RxAcquisitionCount_RBV`

### Removed

- **Breaking**: Remove unused backend status fields (Álvaro Costa)
  - Removed PVs are:
    - `RxError_RBV`
    - `IndexError_RBV`
    - `IndexSentFramesCounter_RBV`
    - For each detector module `<n>`:
      - `M<n>:Backend_BufferUsed_RBV`
      - `M<n>:RxError_RBV`
- Drop installation scripts (Henrique F. Simoes)
  - These aren't used anymore. Remove them, since each site handles
    installation their own way.

### Fixed

- Prevent uninitialized buffer reading in disabled sensors (Henrique F. Simoes)

## Previous releases

Users interested in releases prior to and including 2.5.2-7 should refer to the
repository history or annotated tags when they exist to understand what
changed.
