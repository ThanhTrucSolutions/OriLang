# Arrays in Ori

```ori
hold xs = [10, 20, 30]
xs[1] = 99
push (xs, 40)

hold i = 0
loop i < len xs {
    say ("xs[" + str i + "] = " + str xs[i])
    i = i + 1
}
```

Expected output order: `10`, `99`, `30`, `40`.

```bat
ori run samples/console
```
