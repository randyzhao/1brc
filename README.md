# 1️⃣🐝🏎️ The One Billion Row Challenge

Forked from https://github.com/gunnarmorling/1brc

## Versions


* baseline
* v1: baseline + custom parse function
* v2: direct memory mapping. No multi-threading
* v3: direct memory mapping and multi-threading

Measured on macbook pro with 2.2 GHz 6-Core Intel Core i7.
| Code     | 100m time (s) |  1b time (s) |
|----------|:-------------:|:------------:|
| baseline |    22.1s      |   273.2s     |
| v1       |    18.0s      |   280s       |
| v2       |    10.2s      |   255.8s     |
| v3       |    2.9s       |   58s        |
