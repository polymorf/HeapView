# HeapView
Tool to view heap chunks and memory writes (using pintool)
, strongly inspired by [Villoc](https://github.com/wapiflapi/villoc).

```
/opt/pin/pin -ifeellucky -t pintool/obj-intel64/pintool.so -- ./vuln
HeapView.py trace vuln.svg
```

<img src="https://cdn.rawgit.com/polymorf/HeapView/master/tests/test1.svg">
