# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'account.ui'
#
# Created: Thu Feb 19 04:08:46 2009
#      by: PyQt4 UI code generator 4.4.4
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

class Ui_account(object):
    def setupUi(self, account):
        account.setObjectName("account")
        account.resize(217, 168)
        self.gridLayout = QtGui.QGridLayout(account)
        self.gridLayout.setObjectName("gridLayout")
        self.label_4 = QtGui.QLabel(account)
        self.label_4.setObjectName("label_4")
        self.gridLayout.addWidget(self.label_4, 0, 0, 1, 2)
        self.label = QtGui.QLabel(account)
        self.label.setObjectName("label")
        self.gridLayout.addWidget(self.label, 1, 0, 1, 1)
        self.e_user = QtGui.QLineEdit(account)
        self.e_user.setObjectName("e_user")
        self.gridLayout.addWidget(self.e_user, 1, 1, 1, 1)
        self.label_2 = QtGui.QLabel(account)
        self.label_2.setObjectName("label_2")
        self.gridLayout.addWidget(self.label_2, 2, 0, 1, 1)
        self.e_domain = QtGui.QLineEdit(account)
        self.e_domain.setObjectName("e_domain")
        self.gridLayout.addWidget(self.e_domain, 2, 1, 1, 1)
        self.label_3 = QtGui.QLabel(account)
        self.label_3.setObjectName("label_3")
        self.gridLayout.addWidget(self.label_3, 3, 0, 1, 1)
        self.e_pwd = QtGui.QLineEdit(account)
        self.e_pwd.setObjectName("e_pwd")
        self.gridLayout.addWidget(self.e_pwd, 3, 1, 1, 1)
        self.buttonBox = QtGui.QDialogButtonBox(account)
        self.buttonBox.setOrientation(QtCore.Qt.Horizontal)
        self.buttonBox.setStandardButtons(QtGui.QDialogButtonBox.Cancel|QtGui.QDialogButtonBox.Ok)
        self.buttonBox.setObjectName("buttonBox")
        self.gridLayout.addWidget(self.buttonBox, 4, 0, 1, 2)

        self.retranslateUi(account)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("accepted()"), account.accept)
        QtCore.QObject.connect(self.buttonBox, QtCore.SIGNAL("rejected()"), account.reject)
        QtCore.QMetaObject.connectSlotsByName(account)

    def retranslateUi(self, account):
        account.setWindowTitle(QtGui.QApplication.translate("account", "Set account", None, QtGui.QApplication.UnicodeUTF8))
        self.label_4.setText(QtGui.QApplication.translate("account", "Enter a SIP account to make calls", None, QtGui.QApplication.UnicodeUTF8))
        self.label.setText(QtGui.QApplication.translate("account", "User", None, QtGui.QApplication.UnicodeUTF8))
        self.e_user.setText(QtGui.QApplication.translate("account", "username", None, QtGui.QApplication.UnicodeUTF8))
        self.label_2.setText(QtGui.QApplication.translate("account", "Domain", None, QtGui.QApplication.UnicodeUTF8))
        self.e_domain.setText(QtGui.QApplication.translate("account", "iptel.org", None, QtGui.QApplication.UnicodeUTF8))
        self.label_3.setText(QtGui.QApplication.translate("account", "Password", None, QtGui.QApplication.UnicodeUTF8))
        self.e_pwd.setText(QtGui.QApplication.translate("account", "password", None, QtGui.QApplication.UnicodeUTF8))

