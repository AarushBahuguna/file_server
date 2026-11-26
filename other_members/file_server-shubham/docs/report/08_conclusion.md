# Conclusion

The project shows that a multi-user file system is mostly a coordination problem. Each module is small, but
correctness depends on how they meet: how requests are framed, how locks are ordered, and how long a client
may wait. Freezing the interfaces on day one, building stand-in servers early and giving every module its own
tests allowed four people to work in parallel without waiting for each other.

The most useful lessons were practical. Fixed-size messages below `PIPE_BUF` make concurrent writes to one
pipe safe without any lock. A request id on every message lets a client ignore a late reply instead of
mistaking it for the next answer. Timeouts turn a stuck server into a clear error message. A session table
that returns the open files at logout lets the server release locks of clients that crash.

## Future work

- Run the clients over sockets so that users can connect from other machines.
- Add a proper "directory not empty" status code to the protocol.
- Let the server push a message to a client whose request was chosen as a deadlock victim.
- Add file permissions per user.

TODO(all): after the final integration run, add one paragraph with the end-to-end results.
