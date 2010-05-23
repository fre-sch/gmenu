import gtk
import sys
import os

width = 240
height = 320
items = set()

def read_stdin():
	for line in sys.stdin: items.add( line.strip() )

class View( gtk.TreeView ):
	def __init__( self, *args, **kwargs ):
		super( View, self ).__init__( *args, **kwargs )
		self.set_headers_visible( False )
		self.set_search_column( 0 )
		renderer = gtk.CellRendererText()
		col = gtk.TreeViewColumn( None, renderer, text=0 )
		self.append_column( col )
		self.unset_flags( gtk.CAN_DEFAULT | gtk.CAN_FOCUS )

class Entry( gtk.Entry ):
	def __init__( self, *args, **kwargs ):
		super( Entry, self ).__init__( *args, **kwargs )
		self.set_flags( gtk.CAN_DEFAULT | gtk.CAN_FOCUS )

class Window( gtk.Window ):
	def __init__( self ):
		super( Window, self ).__init__( gtk.WINDOW_POPUP )
		self.set_position( gtk.WIN_POS_CENTER_ALWAYS )
		self.set_default_size( width, height )
		
		self.entry = Entry()
		self.entry.connect( "key-press-event", self.on_entry_key_press )

		model = gtk.ListStore( str )
		for item in items: model.append( (item,) )
		
		self.filter = model.filter_new()
		self.filter.set_visible_func( self.visible_func )
		
		view = View( self.filter )
		sw = gtk.ScrolledWindow()
		sw.set_policy( gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC )
		sw.set_shadow_type( gtk.SHADOW_IN )
		sw.add( view )
		
		lbox = gtk.VBox( False, 0)
		lbox.set_border_width( 2 )
		lbox.pack_start( self.entry, False, True )
		lbox.pack_start( sw, True, True )
	
		box = gtk.Frame()
		box.set_border_width( 0 )
		box.set_shadow_type( gtk.SHADOW_OUT )
		box.add( lbox )
		
		self.add( box )
	
	def show_all( self ):
		super( Window, self ).show_all()
		gtk.gdk.keyboard_grab( self.window )
	
	def on_entry_key_press( self, entry, event ):
		if event.keyval == gtk.keysyms.Escape:
			gtk.main_quit()
		self.filter.refilter()
	
	def visible_func( self, model, itr ):
		search = self.entry.get_text()
		if len( search ):
			return -1 != model.get_value( itr, 0 ).find( search )
		return True

if __name__ == "__main__":
	read_stdin()
	Window().show_all()
	gtk.main()
