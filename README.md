# HeapView
Tool to view heap chunks and memory writes (using pintool)
, strongly inspired by [Villoc](https://github.com/wapiflapi/villoc).

```
/opt/pin/pin -t pintool/obj-intel64/pintool.so -- ./vuln
HeapView.py trace vuln.svg
```

<img src="https://cdn.jsdelivr.net/gh/polymorf/HeapView/tests/test1.svg">
