# `oph` a simple offset patch helper based on C++20

## Dependencies
|library|for what|
|---|---|
|[__fmt__](https://github.com/fmtlib/fmt)|output stream format|
|[__zydis__](https://github.com/zyantific/zydis)|x86/x86-64 disassemble|

## Examples

> **Signature scan**

https://github.com/Akrobatik/oph/blob/b1f31e19d419e7655c7386a8c03bea464348c264/examples/sigexpr.cpp#L7-L47

```
Match: true

Search[0]: 11de
Search[1]: 11df
Search[2]: 11e0
Search[3]: 11e9

Search: expected total 1, peek 0: 11d7

oph/sigexpr: unexpected search result size: expected(2), result(1)
```
<br/>

> **Signature Scan (String)**

https://github.com/Akrobatik/oph/blob/50438d37586b976c76d05335a4bc782925dd778b/examples/sigexpr_string.cpp#L7-L22

```
Match: Hello, world! welcome.: true

Search: My name is Akrobatik. This is oph.: 2c
```
