#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# vim:et ts=4 sw=4:

import gobject
import gtk
import sys
import os
from fnmatch import fnmatch, fnmatchcase
from optparse import OptionParser

def model_get_iter_last(model, parent=None):
  """Returns a gtk.TreeIter to the last row or None if there aren't any rows.
  If parent is None, returns a gtk.TreeIter to the last root row."""
  n = model.iter_n_children(parent)
  return n and model.iter_nth_child(parent, n - 1)

def match_substr(pattern, subject, use_case):
    """Match a string by simple substring."""
    if use_case:
        return subject.find(pattern) != -1
    return subject.lower().find(pattern.lower()) != -1

def match_glob(pattern, subject, use_case):
    """Match a string using shell style globbing."""
    if use_case:
        return fnmatchcase(subject, pattern)
    return fnmatch(subject, pattern)

class GMenu(gtk.Window):
    def __init__(self):
        super(GMenu, self).__init__(gtk.WINDOW_TOPLEVEL)
        self.init_options()
        self.set_position(gtk.WIN_POS_CENTER)
        self.set_default_size(240, 320)
        self.set_type_hint(gtk.gdk.WINDOW_TYPE_HINT_DIALOG);
        self.set_border_width(2)
        
        # these are needed to add a small delay to the filtering of the
        # tree view model
        self.queue_refilter_id = 0
        self.on_entry_key_press_id = 0

        self.init_entry()
        self.init_model()
        sw = self.init_view()

        lbox = gtk.VBox(False, 2)
        lbox.pack_start(self.entry, False, True)
        lbox.pack_start(sw, True, True)
        self.add(lbox)

    def init_options(self):
        parser = OptionParser()
        parser.add_option("-i",
            action="store_true", dest="case", default=False,
            help="use case when matching items")
        parser.add_option("-r",
            action="store_const", dest="mode", const="regex",
            help="use regular expression for matching items")
        parser.add_option("-g",
            action="store_const", dest="mode", const="glob",
            help="use shell style globbing for matching items")
        self.options, self.args = parser.parse_args(sys.argv)
        self.match_fn = match_substr
        if self.options.mode == "glob":
            self.match_fn = match_glob

    def init_entry(self):
        self.entry = gtk.Entry()
        # change behaviour of entry to something more "search box"-like
        self.on_entry_key_press_id = self.entry.connect(
            "key-press-event", self.on_entry_key_press)
        # trigger the refiltering of the model
        self.on_entry_changed_id = self.entry.connect(
            "changed", self.on_entry_changed)

    def init_model(self):
        """Read lines from STDIN and populate the tree model. Also set up the filter."""
        model = gtk.ListStore(str)
        for line in sys.stdin:
            model.append((line.strip(),))
        
        self.filter = model.filter_new()
        self.filter.set_visible_func(self.visible_func)

    def init_view(self):
        self.view = gtk.TreeView(self.filter)
        self.view.set_headers_visible(False)
        self.view.set_search_column(0)
        # selecting a match in the view should update the entry text
        self.view.get_selection().connect("changed",
          self.on_selection_changed)

        renderer = gtk.CellRendererText()
        renderer.ellipsize = True
        self.view.insert_column_with_data_func(
            -1, "Data", renderer, self.cellrenderer_data_func)

        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        sw.set_shadow_type(gtk.SHADOW_IN)
        sw.add(self.view)
        return sw

    def on_selection_changed(self, sel):
        mdl, itr = sel.get_selected()
        if itr:
            item = mdl.get_value(itr, 0)
            self.view.scroll_to_cell(mdl.get_path(itr))
            
            # block temporarily so setting to first match and setting the
            # selection won't trigger a refiltering
            self.entry.handler_block(self.on_entry_changed_id)
            self.entry.set_text(item)
            # select the match in the entry, for easy retyping
            self.entry.select_region(0, -1)
            self.entry.handler_unblock(self.on_entry_changed_id)

    def on_entry_changed(self, entry):
        # if text was typed before the timeout expired, remove the last timeout
        if self.queue_refilter_id:
            gobject.source_remove(self.queue_refilter_id)
        # add a timeout to not immediately refilter in case user isn't done
        # typing.
        self.queue_refilter_id = gobject.timeout_add(300, self.refilter)

    def refilter(self):
        self.filter.refilter()
        self.queue_refilter_id = 0

    def select_next(self):
        """Select next (from current selection) or first item."""
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        itrok = False
        if itr:
            # select next item
            itr = mdl.iter_next(itr)
            itrok = itr is not None
        if not itrok:
            # there was no selection so next item really is first item
            itr = mdl.get_iter_first()
            itrok = itr is not None
        if itrok:
            # tree model was not empty and some item can be selected
            sel.select_iter(itr)

    def select_last(self):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        itr = model_get_iter_last(mdl)
        if itr:
            # tree model was not empty
            sel.select_iter(itr)

    def select_previous(self):
        """Select previous (from current selection) or last item."""
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        if itr:
            path = mdl.get_path(itr)
            path = (path[0] - 1,)
            if path[0] >= 0:
                sel.select_path(path)
            else:
                self.select_last()
        else:
            self.select_last()

    def on_entry_key_press(self, entry, e):
        if e.keyval == gtk.keysyms.Escape:
            gtk.main_quit()
            return True
        elif e.keyval == gtk.keysyms.Tab or e.keyval == gtk.keysyms.Down:
            self.select_next()
            return True
        elif e.state & gtk.gdk.SHIFT_MASK and e.keyval == gtk.keysyms.ISO_Left_Tab or e.keyval == gtk.keysyms.Up:
            self.select_previous()
            return True
        elif e.keyval == gtk.keysyms.Return:
            self.print_result()
            gtk.main_quit()
            return True
        return False

    def print_result(self):
        sel = self.view.get_selection()
        mdl, itr = sel.get_selected()
        if itr:
            print mdl.get_value(itr, 0)
        else:
            print self.entry.get_text()

    def visible_func(self, model, itr):
        pattern = self.entry.get_text()
        if len(pattern):
            subject = model.get_value(itr, 0)
            return self.match_fn(pattern, subject, self.options.case)
        return True

    def cellrenderer_data_func(self, col, cell, mdl, itr):
        if self.options.mode is not None:
            item = mdl.get_value(itr, 0)
            cell.set_property("markup", item)
        else:
            search = self.entry.get_text()
            search_len = len(search)
            item = mdl.get_value(itr, 0)
            lead_p = item.lower().find(search.lower())
            if search and lead_p != -1:
                lead = item[ : lead_p ]
                match = item[ lead_p : lead_p + search_len ]
                tail = item[ lead_p + search_len : ]
                cell.set_property("markup", lead + "<b>" + match + "</b>" + tail)
            else:
                cell.set_property("markup", item)

if __name__ == "__main__":
    GMenu().show_all()
    gtk.main()
