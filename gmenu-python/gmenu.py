#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# vim:et ts=4 sw=4:
import gobject
import gtk
import sys
import os

class GMenu( gtk.Window ):
    def __init__( self ):
        super( GMenu, self ).__init__( gtk.WINDOW_TOPLEVEL )
        self.set_position( gtk.WIN_POS_CENTER )
        self.set_default_size( 240, 320 )
        self.set_type_hint( gtk.gdk.WINDOW_TYPE_HINT_DIALOG );
        self.set_border_width( 2 )
        
        self.queue_refilter_id = 0
        self.on_entry_key_press_id = 0

        self.init_entry()
        self.init_model()
        sw = self.init_view()

        lbox = gtk.VBox( False, 2 )
        lbox.pack_start( self.entry, False, True )
        lbox.pack_start( sw, True, True )
        self.add( lbox )

    def init_entry( self ):
        self.entry = gtk.Entry()
        self.on_entry_key_press_id = self.entry.connect(
            "key-press-event", self.on_entry_key_press )
        self.on_entry_changed_id = self.entry.connect(
            "changed", self.on_entry_changed )

    def init_model( self ):
        model = gtk.ListStore( str )
        for line in sys.stdin:
            model.append( (line.strip(),) )
        
        self.filter = model.filter_new()
        self.filter.set_visible_func( self.visible_func )

    def init_view( self ):
        self.view = gtk.TreeView( self.filter )
        self.view.set_headers_visible( False )
        self.view.set_search_column( 0 )
        self.view.get_selection().connect( "changed",
          self.on_selection_changed )

        renderer = gtk.CellRendererText()
        renderer.ellipsize = True
        self.view.insert_column_with_data_func(
            -1, "Data", renderer, self.cellrenderer_data_func )

        sw = gtk.ScrolledWindow()
        sw.set_policy( gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC )
        sw.set_shadow_type( gtk.SHADOW_IN )
        sw.add( self.view )
        return sw

    def on_selection_changed( self, sel ):
        mdl, itr = sel.get_selected()
        if itr:
            item = mdl.get_value( itr, 0 )
            self.entry.handler_block( self.on_entry_changed_id )
            self.entry.set_text( item )
            self.entry.select_region( 0, -1 )
            self.entry.handler_unblock( self.on_entry_changed_id )

    def on_entry_changed( self, entry ):
        if self.queue_refilter_id:
            gobject.source_remove( self.queue_refilter_id )
        self.queue_refilter_id = gobject.timeout_add( 300, self.refilter )

    def refilter( self ):
        self.filter.refilter()
        self.queue_refilter_id = 0

    def select_next( self ):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        itrok = False
        if itr:
            itr = mdl.iter_next( itr )
            itrok = itr is not None
        if not itrok:
            itr = mdl.get_iter_first()
            itrok = itr is not None
        if itrok:
            sel.select_iter( itr )

    def select_last( self ):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        n = mdl.iter_n_children( None )
        if n:
            itr = mdl.iter_nth_child( None, n - 1 )
            if itr:
                self.view.scroll_to_cell( mdl.get_path( itr ) )
                sel.select_iter( itr )

    def select_previous( self ):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        if itr:
            path = mdl.get_path( itr )
            path = ( path[0] - 1, )
            if path[0] >= 0:
                sel.select_path( path )
            else:
                self.select_last()
        else:
            self.select_last()

    def on_entry_key_press( self, entry, e ):
        if e.keyval == gtk.keysyms.Escape:
            gtk.main_quit()
            return True
        elif e.keyval == gtk.keysyms.Tab or e.keyval == gtk.keysyms.Down:
            self.select_next()
            return True
        elif e.keyval == gtk.keysyms.Up:
            self.select_previous()
            return True
        elif e.keyval == gtk.keysyms.Return:
            self.print_result()
            gtk.main_quit()
            return True
        return False

    def print_result( self ):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        if itr:
            print mdl.get_value( itr, 0 )
        else:
            print self.entry.get_text()

    def visible_func( self, model, itr ):
        search = self.entry.get_text()
        if len( search ):
            return -1 != model.get_value( itr, 0 ).lower().find( search.lower() )
        return True

    def cellrenderer_data_func( self, col, cell, mdl, itr ):
        search = self.entry.get_text()
        search_len = len( search )
        item = mdl.get_value( itr, 0 )
        lead_p = item.lower().find( search.lower() )
        if search and lead_p != -1:
            lead = item[ : lead_p ]
            match = item[ lead_p : lead_p + search_len ]
            tail = item[ lead_p + search_len : ]
            cell.set_property( "markup", lead + "<b>" + match + "</b>" + tail )
        else:
            cell.set_property( "markup", item )

if __name__ == "__main__":
    GMenu().show_all()
    gtk.main()
