import gtk
import sys
import os
import time

width = 240
height = 320
items = set()

class Entry( gtk.Entry ):
  def __init__( self, *args, **kwargs ):
    super( Entry, self ).__init__( *args, **kwargs )
    self.set_flags( gtk.CAN_FOCUS | gtk.CAN_DEFAULT )
    self.grab_focus()


class Window( gtk.Window ):
  def __init__( self, *args, **kwargs ):
    super( Window, self ).__init__( gtk.WINDOW_POPUP )
    self.set_default_size( 240, -1 )
    self.set_position( gtk.WIN_POS_CENTER_ALWAYS )

    entry = Entry()

    menu = gtk.MenuBar()
    menu.set_pack_direction( gtk.PACK_DIRECTION_TTB )
    for i in items:
      item = gtk.MenuItem( i )
      menu.append( item )

    box = gtk.VBox( 0, False )
    box.pack_start( entry, False, False )
    box.pack_start( menu )

    self.add( box )
    self.show_all()


for line in sys.stdin:
  items.add( line.strip() )
Window()
gtk.main()
