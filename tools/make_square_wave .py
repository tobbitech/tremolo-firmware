import math
from matplotlib import pyplot as plt

y = []
x = []


res = 128

print("{", end="")

for i in range(res):
    if (i < res/2):
        n = 0
    else:
        n = 255
    y.append(n)
    x.append(i)
    print(f"{n:.0f}", end="")
    if ( i < res - 1):
        print(", ", end="")

print("}")    


plt.plot(x, y)
plt.show()