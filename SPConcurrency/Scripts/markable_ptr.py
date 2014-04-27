#!/usr/bin/python
#
#  markable_ptr.py
#  Peter Zhivkov.
#
#  Created by Peter Zhivkov on 14/02/2014.
#  Copyright (c) 2014 Peter Zhivkov. All rights reserved.

#
# Add the following to ~/.lldbinit:
#
#    command script import "~/$PROJECT_PATH/markable_ptr.py"
#    script markable_ptr.markable_ptr_UnderlyingType = '_SPCLockFreeListNode'
#
# (e.g. "command script import "~/Dev/Project/Project/markable_ptr.py")
#


import lldb
import lldb.formatters.Logger


class markable_ptr_SynthProvider:

	def __init__(self, valobj, dict):
		logger = lldb.formatters.Logger.Logger()
		self.valobj = valobj

	def num_children(self):
		logger = lldb.formatters.Logger.Logger()
		try:
			return self.node.GetNonSyntheticValue().GetNumChildren()
		except:
			return 0

	def get_child_index(self, name):
		logger = lldb.formatters.Logger.Logger()
		try:
			return self.node.GetIndexOfChildWithName(name)
		except:
			return -1

	def get_child_at_index(self, index):
		logger = lldb.formatters.Logger.Logger()	
		try:
			return self.node.GetChildAtIndex(index)
		except:
			return None

	def update(self):
		logger = lldb.formatters.Logger.Logger()
		try:
			#
			# Get the proper type to cast the pointer to.
			#
			target  = lldb.debugger.GetSelectedTarget()
			utype   = target.FindFirstType(markable_ptr_UnderlyingType)
			ptrType = utype.GetPointerType()
			
			#
			# Check if the pointer is marked, and create an object to a valid memory reference. Otherwise, just cast, dereference, and hold the object.
			#
			self.marked = self.valobj.GetValueAsUnsigned() & 1
			if self.marked:
				self.node    = self.valobj.CreateValueFromAddress(self.valobj.GetName(), self.valobj.GetValueAsUnsigned() - 1, utype)
				self.nodePtr = self.node.AddressOf().Cast(ptrType)
			else:
				self.nodePtr = self.valobj.Cast(ptrType)
				self.node    = self.nodePtr.Dereference()

		except:
			pass

	def has_children(self):
		logger = lldb.formatters.Logger.Logger()
		try:
			return self.nodePtr.GetValueAsUnsigned() != 0 and self.node.GetNonSyntheticValue().GetNumChildren() > 0
		except:
			return False

def markable_ptr_SummaryProvider(valobj, dict):
	marked = valobj.GetValueAsUnsigned() & 1
	ptr    = hex(valobj.GetValueAsUnsigned() - marked)
	if marked == 1:
		return str(ptr) + "*"
	else:
		return ptr


def __lldb_init_module(debugger, dict):
	logger = lldb.formatters.Logger.Logger()
	debugger.HandleCommand('type format add -f pointer markable_ptr_t')
	debugger.HandleCommand('type summary add -F markable_ptr.markable_ptr_SummaryProvider markable_ptr_t')
	debugger.HandleCommand('type synthetic add markable_ptr_t --python-class markable_ptr.markable_ptr_SynthProvider')


  
