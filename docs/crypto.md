# Cryptography and TLS

Monza uses [openssl](https://www.openssl.org/) to implement cryptographic primitives and the enable TLS support for connections.
It targets the 3.x family of versions, but applications can attempt to use the legacy 1.1.1 API surface by adding the define `OPENSSL_API_COMPAT=10101`.
This latter feature is not part of our testing, so there might be unexpected breakages.

Configuration (`guest-verona-rt/openssl/CMakeLists.txt`)
  * no-legacy: minimize old code that is unlikely to be used.
  * no-ssl3 no-tls1 no-tls1_1 no-tls1_2 no-dtls: focus on TLS 1.3 only.
  * no-cmp no-afalgeng: Monza-specific issues.
  * no-ui-console no-secure-memory no-randfile no-file-storen o-stdio no-posix-io no-sock --with-rand-seed=none: removing features that cannot be supported on Monza.

For random number generation Monza exposes a custom implementation of random pools based on rdseed. This code can be found in `guest-verona-rt/openssl/crypto/rng.cc`.
