## Summary

Describe the change and why it is needed.

## Validation

- [ ] `cmake --build --preset windows-vst3` and `ctest --preset windows-vst3`
- [ ] Linux development: `xvfb-run -a ctest --preset linux-dev`
- [ ] Effect response, bypass/automation, preset and host-state round trips
- [ ] Offline editor rendering and local NAM/IR loading
- [ ] FL Studio x64: scanning, project restore, automation and offline render (or explicitly untested)

## Compatibility

- [ ] Existing parameter IDs and state compatibility preserved
- [ ] No network/account dependency introduced
- [ ] README / Windows instructions updated if needed
