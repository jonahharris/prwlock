# Partitioned Read-Write Lock

In many systems, it's common to use a partitioned lock to reduce contention. Rather than a single hash table, for example, we could split the hash table across a number of partitions and perform a hash to determine the partition of a key, then use. In this way, rather than wrapping a hash table with a single read-write lock, we can split up the hash table by a number of partitions and lock only segments of the hash table at once.
