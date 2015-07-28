from random import randint, random

# variables: block_vertical block_horizontal road_vertical road_horizontal
# start: block_vertical
# rules: (block_vertical -> block_horizontal road_vertical block_horizontal)
#        (block_horizontal -> block_vertical road_horizontal block_vertical)

block_vertical = 1
block_horizontal = 2
road_vertical = 10
road_horizontal = 11

road_width = 2
min_width = road_width + 4
min_height = road_width + 4

def as_string(data):
    if data == block_vertical:
        return "block_vertical"
    elif data == block_horizontal:
        return "block_horizontal"
    elif data == road_vertical:
        return "road_vertical"
    return "road_horizontal"

class Tree(object):
    def __init__(self):
        self.left = None
        self.right = None
        self.data = None

def recurse_tree(root, num_iterations, x, y, width, height):
    if num_iterations == 0: return
    if root.data[0] == block_vertical and width > min_width:
        w = int(width * random())/2
        if w < min_width: w = min_width
        root.data = (road_vertical, x+w, y, road_width, height)
        root.left = Tree()
        root.left.data = (block_horizontal, x, y, w-road_width, height)
        root.right = Tree()
        root.right.data = (block_horizontal, x+w+road_width, y, width - w - road_width, height)
        recurse_tree(root.left, num_iterations-1, x, y, w, height)
        recurse_tree(root.right, num_iterations-1, x+w+road_width, y, width - w - road_width, height)
    elif root.data[0] == block_horizontal and height > min_height:
        h = int(height * random())/2
        if h < min_height: h = min_height
        root.data = (road_horizontal, x, y+h, width, road_width)
        root.left = Tree()
        root.left.data = (block_vertical, x, y, width, h-road_width)
        root.right = Tree()
        root.right.data = (block_vertical, x, y+h+road_width, width, height - h - road_width)
        recurse_tree(root.left, num_iterations-1, x, y, width, h)
        recurse_tree(root.right, num_iterations-1, x, y+h+road_width, width, height - h - road_width)

def print_tree(root):
    if root == None: return
    print_tree(root.left)
    print "%s" % as_string(root.data)
    print_tree(root.right)

def create_grid(root, output):
    if root == None: return output
    create_grid(root.left, output)
    x = root.data[1]
    y = root.data[2]
    w = root.data[3]
    h = root.data[4]
    print "%d, %d, %d, %d %s" % (x, y, w, h, as_string(root.data[0]))
    for m in range(y, y+h):
        for n in range(x, x+w):
            if root.data[0] == block_vertical:
                output[m][n] = '+'
            elif root.data[0] == block_horizontal:
                output[m][n] = '+'
            elif root.data[0] == road_vertical:
                output[m][n] = ' '
            elif root.data[0] == road_horizontal:
                output[m][n] = ' '
    create_grid(root.right, output)
    return output
    
def main(width, height, num_iterations=10):
    root = Tree()
    root.data = (block_vertical, width)
    recurse_tree(root, num_iterations, 0, 0, width, height)
    output = []
    for i in range(0, height):
        output.append([])
        for j in range(0, width):
            output[-1].append(' ')
    output = create_grid(root, output)
    #print_tree(root)
    return output
    
if __name__ == '__main__':
    res = main(120, 50, 4)
    for row in res: 
        print ''.join(row)
