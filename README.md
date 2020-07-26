## Containers

Experiments in high(er)-performance data structures in C.

- [cuckoo](./cuckoo) A single-threaded hashmap utilizing cuckoo hashing.
- [flat_map](./flat-map) A concurrent hashmap utilizing open addressing with linear probing and supporting configurable concurrency parameters. This implementation is somewhat limited in the sense that its API does not support generic key types but rather limits keys to 64-bit integers.
- [hashmap](./hashmap) A concurrent hashmap utilizing separate chaining. This implementation is more general than `flat_map` in that generic key types are supported. Additionally, the API for this map supports a higher degree of configuration via a `hashmap_attr` type, while maintaining relative ease-of-use in the common case by supporting a default constructor that initializes the attributes of the map with sensible defaults. 
- [rcu](./rcu) A multi-reader, single-writer RCU memory reclamation system.
- [sync](./sync) Assorted higher-level synchronization constructs.