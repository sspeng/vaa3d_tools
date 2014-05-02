/* prunning_swc_plugin.h
 * This is a test plugin, you can use it as a demo.
 * 2014-05-02 : by Zhi Zhou
 */
 
#ifndef __PRUNNING_SWC_PLUGIN_H__
#define __PRUNNING_SWC_PLUGIN_H__

#include <QtGui>
#include <v3d_interface.h>

class prunning_swc : public QObject, public V3DPluginInterface2_1
{
	Q_OBJECT
	Q_INTERFACES(V3DPluginInterface2_1);

public:
	float getPluginVersion() const {return 1.1f;}

	QStringList menulist() const;
	void domenu(const QString &menu_name, V3DPluginCallback2 &callback, QWidget *parent);

	QStringList funclist() const ;
	bool dofunc(const QString &func_name, const V3DPluginArgList &input, V3DPluginArgList &output, V3DPluginCallback2 &callback, QWidget *parent);
};

#endif
