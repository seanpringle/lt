# LT - Lua Tweaked

A rainy ~~day~~ week experiment to see what Lua syntax would be like with:

- [x] Local variables by default
- [x] An array type
- [x] 0-based indexing
- [x] Integer 0 is false
- [x] Tables can store `nil`
- [x] New tables' metatable defaults to `table`
- [x] Assignment is an expression (returns value)
- [x] Ternary operator (inline-if expression)
- [x] Pattern syntax is PCRE
- [x] Stricter prototype-style inheritance
- [x] Userdata are all light pointers
- [x] Reference counting
- [ ] Concurrent cycle detection (heh!)
- [ ] Avoid use of `longjmp`
- [ ] Rethinking heredoc and `[[...]]`

Don't think of LT as anything but an experiment. Bugs abound, and there's a heap of stuff not done (yet, or maybe ever...):

- No iterators
- No metamethods
- No coroutines
- No closures
- Standard libraries have big gaps or are missing entirely
- Only numeric `for` exists, and is limited to a simple 1+ counter
- Not (easily) embeddable

## Local variables by default

```lua
function hello ()
  text = "hello world"
  print(text)
end

hello()
print(text) -- error
```

Global variables are writable via a `global` table:

```lua
function hello ()
  global.text = "hello world"
  print(text)
end

hello()
print(text) -- success
```

## An array type

Dynamic arrays (tuple, vector, etc) passed by reference.

```lua
stuff = [1,2,3]

for i in #stuff do
  print(stuff[i])
end

stuff[#stuff] = 4
```

The `[]` notation plays funny with Lua `[[...]]` strings. Adding spaces is a workaround, but feels wrong, so `[[...]]` isn't implemented yet.

## 0-based indexing

Lua's 1-based indexing only seemed a bother when interacting with other stuff in the C-derived ecosystem, but there's a lot of that around. This affects the `array` type and strings.

## Integer 0 is false

Lua treats only `nil` and `false` as false. LT adds `0` to the mix.

## Tables can store `nil`

```lua
stuff = { a = 1, b = nil }
print(stuff)
```

```
{a: 1, b: nil}
```

Obviously this means LT can't delete table items by setting them to `nil`, as Lua does. Still thinking about this...

## Assignment as expression

```lua
while line = io.read() do
  print(line)
end
```

## Ternary operator (inline-if)

```lua
print(if true then "yes" else "no" end)
```

## Stricter prototype-style inheritance

*Stricter* because it currently omits a bunch of cool stuff one can do with Lua's metatables and metamethods. Below, `inherit` is the equivalent of calling `setmetatable` and setting `__index`.

```lua
a = {
  alpha = function ()
    print("alpha!")
  end
}

b = inherit(a, {
  beta = function ()
    print("beta!")
  end
})

b.alpha()
```
