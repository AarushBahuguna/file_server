# Team Integration Checklist

- [ ] Agree dispatcher command semantics and maximum write payload.
- [ ] Confirm whether clients use persistent descriptors or stateless path/offset operations.
- [ ] Map `disk_read`/`disk_write` to the cache API; preserve `disk_raw_read`/`disk_raw_write` as scheduler backend.
- [ ] Confirm scheduler thread ownership of raw disk calls.
- [ ] Confirm lock ordering across file, metadata, cache, and scheduler locks.
- [ ] Ensure cache flush precedes `disk_save` at shutdown.
- [ ] Run `make test` through the full cache/scheduler/dispatcher stack.
- [ ] Run concurrent same-directory create/delete tests and restart/persistence tests.
- [ ] Run Valgrind and AddressSanitizer on the integrated executable.
- [ ] Replace CSV placeholders with measured results; do not present placeholders as experimental data.
