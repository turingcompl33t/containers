## Containers

Experiments in high(er)-performance data structures in C.

- [cuckoo](./cuckoo) A single-threaded hashmap utilizing cuckoo hashing.
- [flat_map](./flat-map) A concurrent hashmap utilizing open addressing with linear probing and supporting configurable concurrency parameters. This implementation is somewhat limited in the sense that its API does not support generic key types but rather limits keys to 64-bit integers.
- [hashmap](./hashmap) A concurrent hashmap utilizing separate chaining. This implementation is more general than `flat_map` in that generic key types are supported. Additionally, the API for this map supports a higher degree of configuration via a `hashmap_attr` type, while maintaining relative ease-of-use in the common case by supporting a default constructor that initializes the attributes of the map with sensible defaults. 
- [rcu](./rcu) A multi-reader, single-writer RCU memory reclamation system.
- [rcu_list](./rcu-list) A linked-list implementation that is maintained by the RCU algorithm. Only a single thread may modify the list at any one time, but any number of readers may be simultaneously active and never block writers. As a reader traverses the list in an iteration or find operation concurrently with a mutating operation, it may witness the old state or the new state, but never one that is invalid or corrupt. As a consequence of the use of RCU, items that are erased from the list by writers are never destroyed until all readers who may have witnesses the item have completed their operation, so outstanding iterators into the list are never invalidated by writes.
- [sync](./sync) Assorted higher-level synchronization constructs.