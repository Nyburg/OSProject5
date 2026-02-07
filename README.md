# OSProject5
## Q1
The header stores shared metadata. If multiple processes updated the header concurrently without locking race conditions could occur where two processes write to the same record location or corrupt the record count, resulting in lost or overwritten log entries.
## Q2
Without the record lock, another process could read a record while it is only partially written. A reader might observe incomplete or mixed data, which could result in a corrupted or completely random output. 