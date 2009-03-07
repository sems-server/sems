# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'participant.ui'
#
# Created: Thu Feb 19 04:07:37 2009
#      by: PyQt4 UI code generator 4.4.4
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

class Ui_participant(object):
    def setupUi(self, participant):
        participant.setObjectName("participant")
        participant.resize(115, 141)
        participant.setFloating(True)
        participant.setFeatures(QtGui.QDockWidget.AllDockWidgetFeatures)
        participant.setAllowedAreas(QtCore.Qt.AllDockWidgetAreas)
        self.dockWidgetContents = QtGui.QWidget()
        self.dockWidgetContents.setObjectName("dockWidgetContents")
        self.gridLayout = QtGui.QGridLayout(self.dockWidgetContents)
        self.gridLayout.setObjectName("gridLayout")
        self.frame = QtGui.QFrame(self.dockWidgetContents)
        self.frame.setFrameShape(QtGui.QFrame.StyledPanel)
        self.frame.setFrameShadow(QtGui.QFrame.Raised)
        self.frame.setObjectName("frame")
        self.gridLayout_2 = QtGui.QGridLayout(self.frame)
        self.gridLayout_2.setObjectName("gridLayout_2")
        self.l_status = QtGui.QLabel(self.frame)
        self.l_status.setObjectName("l_status")
        self.gridLayout_2.addWidget(self.l_status, 0, 0, 1, 1)
        self.bt_ciao = QtGui.QPushButton(self.frame)
        self.bt_ciao.setObjectName("bt_ciao")
        self.gridLayout_2.addWidget(self.bt_ciao, 2, 0, 1, 1)
        self.cb_muted = QtGui.QCheckBox(self.frame)
        self.cb_muted.setObjectName("cb_muted")
        self.gridLayout_2.addWidget(self.cb_muted, 1, 0, 1, 1)
        self.gridLayout.addWidget(self.frame, 0, 0, 1, 1)
        participant.setWidget(self.dockWidgetContents)

        self.retranslateUi(participant)
        QtCore.QMetaObject.connectSlotsByName(participant)

    def retranslateUi(self, participant):
        participant.setWindowTitle(QtGui.QApplication.translate("participant", "mapf", None, QtGui.QApplication.UnicodeUTF8))
        self.l_status.setText(QtGui.QApplication.translate("participant", "status", None, QtGui.QApplication.UnicodeUTF8))
        self.bt_ciao.setText(QtGui.QApplication.translate("participant", "ciao", None, QtGui.QApplication.UnicodeUTF8))
        self.cb_muted.setText(QtGui.QApplication.translate("participant", "muted", None, QtGui.QApplication.UnicodeUTF8))

