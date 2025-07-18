import math
from matplotlib import pyplot as plt

y = []
x = []


res = 128

print("{", end="")

for i in range(res):
    n =  (i/res) * 255
    y.append(n)
    x.append(i)
    print(f"{n:.0f}", end="")
    if ( i < res - 1):
        print(", ", end="")

print("}")    


plt.plot(x, y)
plt.show()