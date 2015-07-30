"""Create a map and render it as a pgm image.

(see http://netpbm.sourceforge.net/doc/pgm.html)

(c) Casey Duncan, https://gist.github.com/caseman/8554090
"""

import sys
import math
import json
from functools import lru_cache
from noise import snoise2
import png

MAP_SIZE = 1024
MAP_SEED = 4

FAULT_SCALE = 3.5
FAULT_OCTAVES = 5
FAULT_THRESHOLD = 0.95
FAULT_EROSION_SCALE = 10
FAULT_EROSION_OCTAVES = 8
FAULT_SCALE_F = FAULT_SCALE / MAP_SIZE
ERODE_SCALE_F = FAULT_EROSION_SCALE / MAP_SIZE

LAND_MASS_SCALE = 1.5
EQUITORIAL_MULTIPLIER = 2.5
COAST_COMPLEXITY = 12
WATER_LEVEL = 0.025
COAST_THRESHOLD = 0.02
MOUNTAIN_FAULT_THRESHOLD = 0.08
MOUNTAIN_HILL_THRESHOLD = 0.4
LAND_SCALE_F = LAND_MASS_SCALE / MAP_SIZE

HILL_SCALE = 5.0
HILL_OCTAVES = 8
HILL_THRESHOLD = 0.19
HILL_SCALE_F = HILL_SCALE / MAP_SIZE

MOISTURE_REACH = 0.1
MOISTURE_REACH_TILES = round(MOISTURE_REACH * MAP_SIZE)
RAINFALL_INFLUENCE = 0.03
RAINFALL_INFLUENCE_TILES = round(RAINFALL_INFLUENCE * MAP_SIZE)
RAINFALL_HILL_FACTOR = 0.03
RAINFALL_MOUNTAIN_FACTOR = 0.1
RAINFALL_KERNEL_RADIUS = 3

ICE_ALT = 1.0

RAINFALL_MAX = 0


if len(sys.argv) != 2 or '--help' in sys.argv or '-h' in sys.argv:
	print('%s FILE' % sys.argv[0])
	print()
	print(__doc__)
	raise SystemExit

class reify(object):
    """ Use as a class method decorator.  It operates almost exactly like the
    Python ``@property`` decorator, but it puts the result of the method it
    decorates into the instance dict after the first call, effectively
    replacing the function it decorates with an instance variable.  It is, in
    Python parlance, a non-data descriptor.  An example:

    .. code-block:: python

       class Foo(object):
           @reify
           def jammy(self):
               print 'jammy called'
               return 1

    And usage of Foo:

    .. code-block:: text

       >>> f = Foo()
       >>> v = f.jammy
       'jammy called'
       >>> print v
       1
       >>> f.jammy
       1
       >>> # jammy func not called the second time; it replaced itself with 1
    """
    def __init__(self, wrapped):
        self.wrapped = wrapped
        try:
            self.__doc__ = wrapped.__doc__
        except: # pragma: no cover
            pass

    def __get__(self, inst, objtype=None):
        if inst is None:
            return self
        val = self.wrapped(inst)
        setattr(inst, self.wrapped.__name__, val)
        return val

def fault_level(x, y, seed):
    FL = 1.0 - abs(snoise2(x * FAULT_SCALE_F, y * FAULT_SCALE_F, FAULT_OCTAVES, 
        base=seed + 10, repeatx=FAULT_SCALE))
    thold = max(0.0, (FL - FAULT_THRESHOLD) / (1.0 - FAULT_THRESHOLD))
    FL *= abs(snoise2(x * ERODE_SCALE_F, y * ERODE_SCALE_F, FAULT_EROSION_OCTAVES, 0.85, 
        base=seed, repeatx=FAULT_EROSION_SCALE))
    FL *= math.log10(thold * 9.0 + 1.0)
    return FL

def equator_distance(y):
    return abs(MAP_SIZE - y * 2.0) / MAP_SIZE

def base_height(x, y, seed):
    height = snoise2(x * LAND_SCALE_F, y * LAND_SCALE_F, COAST_COMPLEXITY, base=seed, repeatx=LAND_MASS_SCALE)
    return (height + height * math.log10(10.0 * (1.01 - equator_distance(y))) * EQUITORIAL_MULTIPLIER) / (EQUITORIAL_MULTIPLIER + 1.0)

def hilliness(x, y, seed):
    return abs(snoise2(x * HILL_SCALE_F, y * HILL_SCALE_F, HILL_OCTAVES, 0.9, base=seed, repeatx=HILL_SCALE))


TILE_COLORS = {
    "ocean": (0, 0, 150),
    "coast": (64, 64, 255),
    "plain": (32, 150, 64),
    "hill": (100, 100, 0),
    "mountain": (150, 150, 180),
    "ice": (220, 220, 255),
    "tundra": (150, 120, 130),
}

class Tile:
    def __init__(self, **attrs):
        self.__dict__.update(attrs)

    @reify
    def terrain(self):
        alt = self.base_height + self.fault * 0.5

        if alt + alt * self.ruggedness > (1.0 - self.equator_distance) * ICE_ALT:
            return "ice"

        if self.base_height < WATER_LEVEL and alt < WATER_LEVEL + COAST_THRESHOLD:
            if WATER_LEVEL - self.base_height > COAST_THRESHOLD:
                return "ocean"
            else:
                return "coast"
        else:
            if self.ruggedness > MOUNTAIN_HILL_THRESHOLD:
                return "mountain"
            if (self.ruggedness + self.fault > HILL_THRESHOLD 
                and self.ruggedness > self.fault):
                return "hill"
            if self.fault > MOUNTAIN_FAULT_THRESHOLD:
                return "mountain"
            return "plain"

    def color(self):
        color = TILE_COLORS[self.terrain]
        rainfall = self.rainfall
        return (color[0] - rainfall, color[1] - rainfall, color[2] - rainfall)
        return TILE_COLORS[self.terrain]

    def as_dict(self):
        return

def clamp(value, mn, mx):
    if value < mn: return mn
    if value > mx: return mx
    return value

def line(dx, dy):
    """Bresenham's line algorithm"""
    line = []
    dx = round(dx); dy = round(dy)
    x = 0; end_x = x + dx
    y = 0; end_y = y + dy
    sx = -1 if dx < 0 else 1
    sy = -1 if dy < 0 else 1
    adx = abs(dx); ady = abs(dy)
    err = (adx if adx > ady else -ady) // 2
    while x != end_x or y != end_y:
        err2 = err
        if err2 > -adx:
            err -= ady
            x += sx
        if err2 < ady:
            err += adx
            y += sy
        line.append((x, y))
    line.append((x, y))
    return line

def prevailing_wind(y):
    angle = -equator_distance(y) * 2.0 * math.pi
    return math.cos(angle), math.sin(angle)

@lru_cache()
def prevailing_wind_line(y):
    dx, dy = prevailing_wind(y)
    return line(dx * -MOISTURE_REACH_TILES, dy * -MOISTURE_REACH_TILES)

class Map:
    def __init__(self, width, height, seed):
        self.width = width
        self.height = height
        self.seed = seed
        self.tiles = {}
        self.create_tiles()

    def tile_pass(self, func):
        tiles = self.tiles
        for y in range(self.height):
            for x in range(self.width):
                func(x, y, tiles)

    def tile(self, x, y):
        try:
            return self.tiles[x, y]
        except IndexError:
            while x < 0: # wrap x
                x += self.width
            while x >= self.width:
                x -= self.width
            if y < 0: # bounce y
                y = -y
            elif y >= self.height:
                y = (self.height - 1) - (y - self.height)
            return self.tiles[x, y]

    def tile_factory(self, x, y, tiles):
        bh = base_height(x, y, self.seed)
        f = fault_level(x, y, self.seed)
        r = hilliness(x, y, self.seed)
        el = bh + f * 0.5
        is_land = bh >= WATER_LEVEL or el >= WATER_LEVEL + COAST_THRESHOLD;
        tile = Tile(
            equator_distance = equator_distance(y),
            base_height = bh,
            fault = f,
            elevation = el,
            ruggedness = r,
            is_land = is_land,
            air_moisture = 0.0,
            rainfall = 0.0,
        )
        # pre-wrap x, pre-bounce y
        for tx in (x - self.width, x, x + self.width):
            for ty in (-y, y, (self.height - 1) - (y - self.height)):
                tiles[tx, ty] = tile

    def set_rainfall(self, x, y, tiles):
        global RAINFALL_MAX
        if (x - RAINFALL_KERNEL_RADIUS) % RAINFALL_KERNEL_RADIUS != 0 or (y - RAINFALL_KERNEL_RADIUS) % RAINFALL_KERNEL_RADIUS != 0:
            return
        tile = tiles[x, y]
        if not tile.is_land:
            return
        clear_line = True
        moisture = 0.0
        rain_factor = 0.5
        rainfall_reach = RAINFALL_INFLUENCE_TILES
        for wx, wy in prevailing_wind_line(y):
            if clear_line:
                nearby_tile = tiles[x + wx, y + wy]
                terrain = nearby_tile.terrain
                if terrain == 'coast' or terrain == 'ocean':
                    moisture += 1.0
                elif terrain == 'mountain':
                    clear_line = False
                elif terrain != 'ice':
                    moisture += (1.0 - nearby_tile.equator_distance) * (1.0 - nearby_tile.ruggedness)
                else:
                    moisture *= 0.25
            if rainfall_reach:
                terrain = tiles[x - wx, y - wy].terrain
                if terrain == 'hill':
                    rain_factor += RAINFALL_HILL_FACTOR
                elif terrain == 'mountain':
                    rain_factor += RAINFALL_MOUNTAIN_FACTOR
                rainfall_reach -= 1
            elif not clear_line:
                break
        rainfall = rain_factor * moisture
        if rainfall > RAINFALL_MAX: 
            RAINFALL_MAX = rainfall
        for tx in range(x - RAINFALL_KERNEL_RADIUS, x + RAINFALL_KERNEL_RADIUS):
            for ty in range(y - RAINFALL_KERNEL_RADIUS, y + RAINFALL_KERNEL_RADIUS):
                tiles[tx, ty].rainfall = rainfall

    def create_tiles(self):
        self.tile_pass(self.tile_factory)
        self.tile_pass(self.set_rainfall)

    def write_image_ppm(self, filename):
        tiles = self.tiles
        f = open(filename, 'wt')
        f.write('P3\n')
        f.write('%s %s\n' % (self.width, self.height))
        f.write('255\n')
        for y in range(self.height):
            for x in range(self.width):
                c = tiles[x, y].color()
                if isinstance(c, tuple):
                    f.write("%s %s %s\n" % c)
                else:
                    c = int(c * 255)
                    f.write("%s %s %s\n" % (c, c, c))
        f.close()

    def write_image_png(self, filename):
        tiles = self.tiles
        f = open(filename, 'wb')
        w = png.Writer(self.width, self.height)
        img_array = []
        for y in range(self.height):
            row = []
            for x in range(self.width):
                c = tiles[x, y].color()
                if isinstance(c, tuple):
                    row.append(clamp(c[0], 0, 255))
                    row.append(clamp(c[1], 0, 255))
                    row.append(clamp(c[2], 0, 255))
                else:
                    c = int(c * 255)
                    row.append(c)
                    row.append(c)
                    row.append(c)
            img_array.append(tuple(row))
        w.write(f, img_array)
        f.close()
        
    def write_json(self, filename):
        f = open(sys.argv[1], 'wt')
        terrain_array = [
            [self.tiles[x, y].terrain
                for x in range(self.width)] 
                for y in range(self.height)]
        json.dump(terrain_array, f)


def tile_type(x, y, seed):
    f = fault_level(x, y, seed)
    bh = base_height(x, y, seed)
    hills = hilliness(x, y, seed)
    alt = bh + f * 0.5
    eq_dist = equator_distance(y)

    if alt + alt * hills > (1.0 - eq_dist) * ICE_ALT:
        return "ice"

    if bh < WATER_LEVEL and alt < WATER_LEVEL + COAST_THRESHOLD:
        if WATER_LEVEL - bh > COAST_THRESHOLD:
            return "ocean"
        else:
            return "coast"
    else:
        if hills > MOUNTAIN_HILL_THRESHOLD:
            return "mountain"
        if hills + f > HILL_THRESHOLD and hills > f:
            return "hill"
        if f > MOUNTAIN_FAULT_THRESHOLD:
            return "mountain"
        return "plain"

def tile_color(x, y, seed):
    return TILE_COLORS[tile_type(x, y, seed)]

def wind_dir(x, y, seed):
    dx, dy = prevailing_wind(y)
    r = dy
    g = .866 * dx + -.5 * dy
    b = -.866 * dx + -.5 * dy
    if r<0 and g<0 and b<0: print((dx,dy))
    return r > 0 and r * 255, g > 0 and g * 255, b > 0 and b * 255


world_map = Map(MAP_SIZE, MAP_SIZE, MAP_SEED)
world_map.write_image_png(sys.argv[1])

