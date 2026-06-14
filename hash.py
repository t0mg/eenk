A=54059
B=76963
FIRSTH=37
def h(s):
    v=FIRSTH
    for c in s:
        v=(v*A)^(ord(c)*B)
    return v&0xFFFFFFFF

print([(k, h(k)) for k in ['items', 'item', 'Inventory', 'lastUsed', 'did', 'q', 'el', '_list', 'pop']])
