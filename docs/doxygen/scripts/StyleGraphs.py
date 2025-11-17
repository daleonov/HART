import glob
import sys
from os import path

import bs4
from tqdm import tqdm

rose_pine = {
    'base': '#191724',
    'surface': '#1f1d2e',
    'overlay': '#26233a',
    'muted': '#6e6a86',
    'subtle': '#908caa',
    'text': '#e0def4',
    'love': '#eb6f92',
    'gold': '#f6c177',
    'rose': '#ebbcba',
    'pine': '#31748f',
    'foam': '#9ccfd8',
    'iris': '#c4a7e7',
    'highlight low': '#21202e',
    'highlight med': '#403d52',
    'highlight high': '#524f67',
}

svg_file_path = r"D:\Projects\HART\docs\doxygen\html\hart__dsp_8hpp__incl.svg"

def rose_pine_ify(svg_file_path):
    soup = bs4.BeautifulSoup(open(svg_file_path).read(), features='xml')

    # Background
    for polygon in soup.find_all('polygon', fill='white', stroke='transparent'):
        polygon['fill'] = 'none'

    # Base classes
    for polygon in soup.find_all('polygon', fill='white', stroke='#666666'):
        polygon['fill'] = rose_pine['gold']
        polygon['stroke'] = 'none'

    # Undocumented classes
    for polygon in soup.find_all('polygon', fill='#e0e0e0', stroke='#999999'):
        polygon['fill'] = rose_pine['rose']
        polygon['stroke'] = 'none'

    # Inherited classes
    for polygon in soup.find_all('polygon', fill='#999999', stroke='#666666'):
        polygon['fill'] = rose_pine['iris']
        polygon['stroke'] = 'none'

    # Truncated classes
    for polygon in soup.find_all('polygon', fill='#fff0f0', stroke='red'):
        polygon['fill'] = rose_pine['muted']
        polygon['stroke'] = 'none'

    # Text near arrows
    for text in soup.find_all('text', fill='grey'):
        text['fill'] = rose_pine['text']

    # Text in classes
    for element in soup.find_all('text'):
        if not element.has_attr('fill'):
            element['fill'] = rose_pine['overlay']

    # Arrows - begin

    # Public inheritance
    for element in soup.find_all(fill='#63b8ff'):
        element['fill'] = rose_pine['subtle']
    for element in soup.find_all(stroke='#63b8ff'):
        element['stroke'] = rose_pine['subtle']

    # Template
    for element in soup.find_all(fill='orange'):
        element['fill'] = rose_pine['text']
    for element in soup.find_all(stroke='orange'):
        element['stroke'] = rose_pine['text']

    # Used class
    for element in soup.find_all(fill='#9a32cd'):
        element['fill'] = rose_pine['iris']
    for element in soup.find_all(stroke='#9a32cd'):
        element['stroke'] = rose_pine['iris']

    # Private inheritance
    for element in soup.find_all(fill='#8b1a1a'):
        element['fill'] = rose_pine['love']
    for element in soup.find_all(stroke='#8b1a1a'):
        element['stroke'] = rose_pine['love']

    # Protected inheritance
    for element in soup.find_all(fill='darkgreen'):
        element['fill'] = rose_pine['gold']
    for element in soup.find_all(stroke='darkgreen'):
        element['stroke'] = rose_pine['gold']

    # Arrows - end

    for element in soup.find_all('style', type='text/css'):
        element.string.replace_with(element.text.replace('stroke: red', f'stroke: {rose_pine["text"]}').replace('fill: red', f'fill: {rose_pine["text"]}'))

    for element in soup.find_all('g', id='navigator'):
        element['fill']  = rose_pine['pine']

        child_element = element.find('rect', fill='#f2f5e9')
        child_element['fill']  = rose_pine['overlay']
        child_element['fill-opacity'] = '1.0'
        child_element['stroke']  = 'none'

    for use in soup.find_all('use', fill='#404040'):
        use['fill'] = rose_pine['pine']
        use.find('set')['to'] = rose_pine['iris']

    g = soup.find('g', id='arrow_out')
    if g is not None:
        rect = g.find('rect')
        rect['fill'] = rose_pine['pine']
        rect['stroke'] = 'none'
        path = g.find('path')
        path['style'] = f'fill:{rose_pine["text"]}'

    svg_data = soup.prettify("utf-8")
    with open(svg_file_path, "wb") as file:
        file.write(svg_data)

svg_file_wildcards = {
    '*__dep__incl.svg',
    '*__inherit__graph.svg',
    '*__coll__graph.svg',
    '*_dep.svg',
    'graph_legend.svg',

    # '*__dep__incl_org.svg',
    # '*__inherit__graph_org.svg',
    #'*__coll__graph__org.svg',
    #'*_dep_org.svg',
}

svg_file_paths = []
for wildcard in svg_file_wildcards:
    svg_file_paths += glob.glob (path.join (sys.argv[1], wildcard))

for svg_file_path in tqdm(svg_file_paths):
    rose_pine_ify(svg_file_path)
