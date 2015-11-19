#!/usr/bin/env python2
from copy import deepcopy
import random
import sys

class Chunk():
	def __init__(self,start,end,dstart,dend):
		self.start = start
		self.end = end
		self.dstart = dstart
		self.dend = dend
		self.free = False
		self.color = random_color()
	def get_color(self):
		if self.free == False:
			return self.color
		else:
			return (240,240,240)
	def get_start(self):
		return self.start
	def get_dstart(self):
		return self.dstart
	def get_dend(self):
		return self.dend
	def get_end(self):
		return self.end
	def set_free(self):
		self.free = True
	def isFree(self):
		return self.free
	def __repr__(self):
		return "Chunk(%#x,%#x,data(%#x,%#x),free=%d)" % (self.start,self.end,self.dstart,self.dend,self.free)
	def overflow(self,addr):
		if addr > self.start and addr <= self.dstart:
			overflow=addr-self.start
			s="s" if overflow > 1 else ""
			return "next_chunk (%d byte%s)" % (overflow,s)
		if addr > self.dend and addr < self.end:
			overflow=addr-self.dend
			s="s" if overflow > 1 else ""
			return "padding (%d byte%s)" % (overflow,s)
		return None

class MemoryWrite():
	def __init__(self,name,start,end):
		self.name = name
		self.start = start
		self.end = end
	def get_name(self):
		return self.name
	def get_start(self):
		return self.start
	def get_end(self):
		return self.end
	def __repr__(self):
		return "%s(%#x->%#x)" % (self.name,self.start,self.end)

class State():
	def __init__(self,operation,chunks,memory_writes,min_addr,max_addr):
		self.operation=operation
		self.chunks = chunks
		self.memory_writes = memory_writes
		self.max_addr=max_addr
		self.min_addr=min_addr
	def get_chunks(self):
		return self.chunks
	def get_memoryWrites(self):
		return self.memory_writes
	def get_name(self):
		return self.operation.replace("<","&lt;").replace(">","&gt;")
	def get_min_addr(self):
		return self.min_addr
	def get_max_addr(self):
		return self.max_addr

def svg_header(height):
	return '''<?xml version="1.0" encoding="ISO-8859-1" standalone="no"?>
<svg xmlns="http://www.w3.org/2000/svg" version="1.1" xmlns:xlink="http://www.w3.org/1999/xlink" width="1400" height="%d" onload="init(evt)">
''' % (height)

def svg_footer():
	return '''<rect class="tooltip_bg" id="tooltip_bg" x="0" y="0" rx="4" ry="4" width="55" height="17" visibility="hidden"/>
<text class="tooltip" id="tooltip" x="0" y="0" visibility="hidden"><tspan id="tooltip-bold" font-weight="bold">A</tspan><tspan id="tooltip-nobold">B</tspan></text>
</svg>'''

def svg_style():
	return '''<style>
	.caption{
		font-size: 14px;
		font-family: Georgia, serif;
	}
	.tooltip{
		font-size: 16px;
		font-familly: monospace;
	}
	.tooltip_bg{
		fill: white;
		stroke: black;
		stroke-width: 1;
		opacity: 0.85;
	}
	svg {
		padding: 5px;
	}
</style>'''

def svg_script():
	return '''<script type="text/ecmascript">
<![CDATA[

function init(evt)
{
    if ( window.svgDocument == null )
    {
	svgDocument = evt.target.ownerDocument;
    }
    svg = document.querySelector('svg');
    pt = svg.createSVGPoint();


    tooltip = svgDocument.getElementById('tooltip');
    tooltip_nobold = svgDocument.getElementById('tooltip-nobold');
    tooltip_bold = svgDocument.getElementById('tooltip-bold');
    tooltip_bg = svgDocument.getElementById('tooltip_bg');

}

function cursorPoint(evt){
	pt.x = evt.clientX; pt.y = evt.clientY;
	return pt.matrixTransform(svg.getScreenCTM().inverse());
}

function ShowTooltip(evt, boldText, normalText)
{
	if (normalText != "") {
		loc = cursorPoint(evt);
	    tooltip.setAttributeNS(null,"x",loc.x+11);
	    tooltip.setAttributeNS(null,"y",loc.y+27);
	    tooltip_nobold.firstChild.data = normalText;
	    tooltip_bold.firstChild.data = boldText;
	    tooltip.setAttributeNS(null,"visibility","visible");
	    tooltip.setAttributeNS(null,"visibility","visible");

	    length = tooltip.getComputedTextLength();
	    tooltip_bg.setAttributeNS(null,"width",length+8);
	    tooltip_bg.setAttributeNS(null,"x",loc.x+8);
	    tooltip_bg.setAttributeNS(null,"y",loc.y+14);
	    tooltip_bg.setAttributeNS(null,"visibility","visibile");
    }
}

function HideTooltip(evt)
{
    tooltip.setAttributeNS(null,"visibility","hidden");
    tooltip_bg.setAttributeNS(null,"visibility","hidden");
}

]]>
</script>
'''

def svg_rec(x,y,width,height,rx,ry,colors,opacity=1,boldtext="",text=""):
	if width < 4:
		width=4
		x-=2
	svg='''<rect 
	style="stroke:black; stroke-width:1; fill:rgb(%d,%d,%d); fill-opacity:%f;"
	rx="%d" ry="%d" height="%dpx" width="%dpx" y="%d" x="%d" 
	onmousemove="ShowTooltip(evt, '%s', '%s')"
	onmouseout="HideTooltip(evt)"
/>\n''' % (colors[0],colors[1],colors[2],opacity,rx,ry,height,width,y,x,boldtext,text)
	return svg

def svg_dashed_rec(x,y,width,height,rx,ry,colors,opacity=1,boldtext="",text=""):
	svg='''<rect 
	stroke-dasharray="5,5" 
	style="stroke:black; stroke-width:1; fill:rgb(%d,%d,%d); fill-opacity:%f;"
	rx="%d" ry="%d" height="%dpx" width="%dpx" y="%d" x="%d" 
	onmousemove="ShowTooltip(evt, '%s', '%s')"
	onmouseout="HideTooltip(evt)"
/>\n''' % (colors[0],colors[1],colors[2],opacity,rx,ry,height,width,y,x,boldtext,text)
	return svg

def svg_text(x,y,text,bold="100",color=(0,0,0)):
	svg='''<text font-weight="%s" style="fill:rgb(%d,%d,%d);" x="%d" y="%d">%s</text>\n''' % (bold,color[0],color[1],color[2],x,y,text)
	return svg

def svg_info(x,y,text):
	return '''
<g id="popup-group" transform="translate(%d,%d)">
	<path d="M-60,-40L180,-40L180,-20L10,-20L0,-10L-10,-20L-60,-20L-60,-40" id="popup" stroke-linecap="square" style="stroke-width: 2px; stroke: rgb(128, 0, 0); fill: rgb(255,128,128); fill-opacity: 0.95; stroke-opacity: 1;"></path>
	<text class="h2" text-anchor="middle" transform="translate(55, -25)" fill-opacity="1">%s</text>
</g>
''' % (x,y,text)

def parse_arg(arg):
    if arg is None:
        return None
    if arg == "<void>":
        return 0
    try:
    	arg=int(arg,0)
    except:
    	pass
    return arg

def parse_call(call_line):
	args = []
	name = call_line.split("(")[0]
	ret = call_line.split(" = ")[-1]
	call_args = call_line.split("(")[1].split(")")[0].split(",")
	for arg in [ret,]+call_args:
		args.append(parse_arg(arg))
	return (name,args)

def parse_ltrace(data):
	chunks = []
	states = []
	max_addr = 0
	min_addr = 0xFFFFFFFFFFFFFFFF
	
	last_write_function=""
	last_write_function_info=""

	lines = data.split("\n")
	for line in lines:
		memory_writes = None
		if line is "":
			continue
		(call_name,call_args) = parse_call(line)
		if call_name == "malloc":
			chunk_start = call_args[0]-8
			data_start = call_args[0]
			data_end = call_args[0]+call_args[1]
			size = call_args[1] + 8 + (0x10-1)
			size = size - (size%0x10)
			if size < 0x20:
				size=0x20
			chunk_end=chunk_start + size
			chunks.append(Chunk(chunk_start,chunk_end,data_start,data_end))
			if chunk_end > max_addr:
				max_addr=chunk_end
			if chunk_start < min_addr:
				min_addr=chunk_start
		elif call_name == "free":
			# find chunk
			free_ok=False
			for chunk in chunks:
				if chunk.get_dstart() == call_args[1] and not chunk.isFree():
					chunk.set_free()
					free_ok=True
			if not free_ok:
				continue
		elif call_name == "memset":
			start=call_args[1]
			end=call_args[1]+call_args[3]
			for chunk in chunks:
				if start >= chunk.get_dstart() and start <= chunk.get_dend():
					memory_writes=MemoryWrite(call_name,start,end)
					break
		elif call_name == "memcpy":
			start=call_args[1]
			end=call_args[1]+call_args[3]
			for chunk in chunks:
				if start >= chunk.get_dstart() and start <= chunk.get_dend():
					memory_writes=MemoryWrite(call_name,start,end)
					break
		elif call_name == "memory_write":
			start=call_args[2]
			end=start+call_args[3]
			function = call_args[1].split("+")[0]
			if last_write_function == function:
				if last_write_size+last_write_addr == start:
					last_write_size+=call_args[3]
					continue
				else:
					for chunk in chunks:
						if start >= chunk.get_dstart() and start <= chunk.get_dend():
							memory_writes=MemoryWrite(last_write_function,last_write_addr,last_write_addr+last_write_size)
							break
					if last_write_function not in ("_int_malloc","_int_free","malloc_consolidate"):
						states.append(State("<"+last_write_function_info+">",deepcopy(chunks),memory_writes,min_addr,max_addr))
					last_write_addr = start
					last_write_size = call_args[3]
					last_write_function=function
					last_write_function_info=call_args[1]
			else:
				if last_write_function != "":
					for chunk in chunks:
						if start >= chunk.get_dstart() and start <= chunk.get_dend():
							memory_writes=MemoryWrite(last_write_function,last_write_addr,last_write_addr+last_write_size)
							break
					if last_write_function not in ("_int_malloc","_int_free","malloc_consolidate"):
						states.append(State("<"+last_write_function_info+">",deepcopy(chunks),memory_writes,min_addr,max_addr))
				last_write_addr = start
				last_write_size = call_args[3]
				last_write_function=function
				last_write_function_info=call_args[1]
			continue
		else:
			continue
		states.append(State(line,deepcopy(chunks),memory_writes,min_addr,max_addr))

	return states

def state_to_svg(pos,state,min_addr,max_addr):
	svg=""
	line_pos = 100*pos + 5*pos
	svg+=svg_rec(5, line_pos, 1010, 100, 10, 10, (255,255,255))
	svg+=svg_text(10,line_pos+15,state.get_name(),bold="bold")
	for chunk_nb,chunk in enumerate(state.get_chunks()):
		overlap=0
		for nb,check_chunk in enumerate(state.get_chunks()):
			if nb >= chunk_nb:
				break
			if chunk.get_start() >= check_chunk.get_start() and chunk.get_start() < check_chunk.get_end():
				overlap+=1;
		chunk_size = chunk.get_end() - chunk.get_start()
		size = int(((chunk_size * 1.0) / (max_addr - min_addr * 1.0)) * 1000)
		start = int(((chunk.get_start() - min_addr * 1.0) / (max_addr - min_addr * 1.0)) * 1000)
		chunk_dsize = chunk.get_dend() - chunk.get_dstart()
		bold_text = "%#x" % (chunk.get_start())
		text = " - %#x (%#x) (%#x -> %#x)" % (chunk_size,chunk_dsize,chunk.get_start(),chunk.get_end())
		if chunk.isFree():
			svg+=svg_dashed_rec(start+10, line_pos + 20 + overlap*20, size, 20, 5, 5, chunk.get_color(),text=text,boldtext=bold_text)
		else:
			svg+=svg_rec(start+10, line_pos + 20 + overlap*20, size, 20, 5, 5, chunk.get_color(),text=text,boldtext=bold_text)
	write = state.get_memoryWrites()
	if write is not None:
		size = int(((write.get_end() - write.get_start() * 1.0) / (max_addr - min_addr * 1.0)) * 1000)
		start = int(((write.get_start() - min_addr * 1.0) / (max_addr - min_addr * 1.0)) * 1000)
		colors=(128,255,128)
		text_colors=(0,0,0)	
		text="%s (%#x -> %#x)" % (write.get_name(),write.get_start(),write.get_end())
		tooltip_text=""
		overflow=None
		for chunk in state.get_chunks():
			overflow=chunk.overflow(write.get_end())
			if overflow is not None:
				colors=(255,0,0)
				text_colors=(255,255,255)
				tooltip_text="overflow in "+overflow+" "
				break
		# check if end addr is in chunk
		found=False
		for chunk in state.get_chunks():
			if write.get_end() >= chunk.get_start() and write.get_end() <= chunk.get_end():
				found=True
				break
		if not found:
			overflow="write outside chunk"
			colors=(255,0,0)
			text_colors=(255,255,255)
			tooltip_text=overflow


		overlap=-1
		for check_chunk in state.get_chunks():
			if write.get_start() > check_chunk.get_start() and write.get_start() <= check_chunk.get_end():
				overlap+=1;
		if overflow:
			svg+=svg_info(start+10+size, line_pos + 24 + overlap*20, tooltip_text)
		svg+=svg_rec(start+10, line_pos + 24 + overlap*20, size, 12, 2, 2, colors, opacity=1,text=text)
	return svg



def random_color(r=200, g=200, b=125):

    red = (random.randrange(0, 256) + r) / 2
    green = (random.randrange(0, 256) + g) / 2
    blue = (random.randrange(0, 256) + b) / 2

    return (red, green, blue)



if __name__ == '__main__':
	random.seed(100)
	chunks=[]
	data = open(sys.argv[1],"rb").read()
	states = parse_ltrace(data)
	min_addr = states[-1].get_min_addr()
	max_addr = states[-1].get_max_addr()

	nb_state = len(states)
	svg_height=100*nb_state + 5*nb_state + 110
	svg=svg_header(svg_height)
	svg+=svg_style()
	svg+=svg_script()
	for pos,state in enumerate(states):
		svg+=state_to_svg(pos,state,min_addr,max_addr)

	svg+=svg_footer()
	out=open(sys.argv[2],"wb")
	out.write(svg)
	out.close()

