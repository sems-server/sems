# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'callbox.ui'
#
# Created: Thu Feb 19 01:59:28 2009
#      by: PyQt4 UI code generator 4.4.4
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

class Ui_callbox(object):
    def setupUi(self, callbox):
        callbox.setObjectName("callbox")
        callbox.setWindowModality(QtCore.Qt.NonModal)
        callbox.resize(234, 90)
        self.gridLayout = QtGui.QGridLayout(callbox)
        self.gridLayout.setObjectName("gridLayout")
        self.label = QtGui.QLabel(callbox)
        self.label.setObjectName("label")
        self.gridLayout.addWidget(self.label, 0, 0, 1, 1)
        self.num = QtGui.QLineEdit(callbox)
        self.num.setObjectName("num")
        self.gridLayout.addWidget(self.num, 0, 1, 1, 1)
        self.buttonBox = QtGui.QDialogButtonBox(callbox)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtGui.QDialogButtonBox.Cancel|QtGui.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self.gridLayout.addWidget(self.buttonBox, 1, 0, 1, 2)

        self.retranslateUi(callbox)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("accepted()"), callbox.accept)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("rejected()"), callbox.reject)
        QtCore.QMetaObject.connectSlotsByName(callbox)

    def retranslateUi(self, callbox):
        callbox.setWindowTitle(QtGui.QApplication.translate("callbox", "call", None, QtGui.QApplication.UnicodeUTF8))
        self.label.setText(QtGui.QApplication.translate("callbox", "calll", None, QtGui.QApplication.UnicodeUTF8))
        self.num.setText(QtGui.QApplication.translate("callbox", "+49", None, QtGui.QApplication.UnicodeUTF8))

