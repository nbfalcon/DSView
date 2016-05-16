/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#include "protocoldock.h"
#include "../sigsession.h"
#include "../view/decodetrace.h"
#include "../device/devinst.h"
#include "../data/decodermodel.h"
#include "../data/decoderstack.h"
#include "../dialogs/protocollist.h"
#include "../dialogs/protocolexp.h"

#include <QObject>
#include <QHBoxLayout>
#include <QPainter>
#include <QMessageBox>
#include <QFormLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>

#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

namespace pv {
namespace dock {

ProtocolDock::ProtocolDock(QWidget *parent, SigSession &session) :
    QScrollArea(parent),
    _session(session)
{
    _up_widget = new QWidget(this);

    QHBoxLayout *hori_layout = new QHBoxLayout();

    _add_button = new QPushButton(_up_widget);
    _add_button->setFlat(true);
    _add_button->setIcon(QIcon::fromTheme("protocol",
                         QIcon(":/icons/add.png")));
    _del_all_button = new QPushButton(_up_widget);
    _del_all_button->setFlat(true);
    _del_all_button->setIcon(QIcon::fromTheme("protocol",
                             QIcon(":/icons/del.png")));
    _del_all_button->setCheckable(true);
    _protocol_combobox = new QComboBox(_up_widget);

    GSList *l = g_slist_sort(g_slist_copy(
        (GSList*)srd_decoder_list()), decoder_name_cmp);
    for(; l; l = l->next)
    {
        const srd_decoder *const d = (srd_decoder*)l->data;
        assert(d);

        const bool have_probes = (d->channels || d->opt_channels) != 0;
        if (true == have_probes) {
            _protocol_combobox->addItem(QString::fromUtf8(d->name), qVariantFromValue(l->data));
        }
    }
    g_slist_free(l);

    hori_layout->addWidget(_add_button);
    hori_layout->addWidget(_del_all_button);
    hori_layout->addWidget(_protocol_combobox);
    hori_layout->addStretch(1);

    connect(_add_button, SIGNAL(clicked()),
            this, SLOT(add_protocol()));
    connect(_del_all_button, SIGNAL(clicked()),
            this, SLOT(del_protocol()));

    _up_layout = new QVBoxLayout();
    _up_layout->addLayout(hori_layout);
    _up_layout->addStretch(1);

    _up_widget->setLayout(_up_layout);
    _up_widget->setMinimumHeight(120);

//    this->setWidget(_widget);
//    _widget->setGeometry(0, 0, sizeHint().width(), 500);
//    _widget->setObjectName("protocolWidget");

    _dn_widget = new QWidget(this);

    _dn_set_button = new QPushButton(_dn_widget);
    _dn_set_button->setFlat(true);
    _dn_set_button->setIcon(QIcon::fromTheme("protocol",
                             QIcon(":/icons/gear.png")));
    connect(_dn_set_button, SIGNAL(clicked()),
            this, SLOT(set_model()));

    _dn_save_button = new QPushButton(_dn_widget);
    _dn_save_button->setFlat(true);
    _dn_save_button->setIcon(QIcon::fromTheme("protocol",
                             QIcon(":/icons/save.png")));
    connect(_dn_save_button, SIGNAL(clicked()),
            this, SLOT(export_table_view()));

    QHBoxLayout *dn_title_layout = new QHBoxLayout();
    dn_title_layout->addWidget(_dn_set_button, 0, Qt::AlignLeft);
    dn_title_layout->addWidget(_dn_save_button, 0, Qt::AlignLeft);
    dn_title_layout->addWidget(new QLabel(tr("Protocol List Viewer"), _dn_widget), 1, Qt::AlignLeft);
    dn_title_layout->addStretch(1);

    _table_view = new QTableView(_dn_widget);
    _table_view->setModel(_session.get_decoder_model());
    _table_view->setAlternatingRowColors(true);
    _table_view->setShowGrid(false);
    _table_view->horizontalHeader()->setStretchLastSection(true);
    _table_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    QVBoxLayout *dn_layout = new QVBoxLayout();
    dn_layout->addLayout(dn_title_layout);
    dn_layout->addWidget(_table_view);

    _dn_widget->setLayout(dn_layout);
    _dn_widget->setMinimumHeight(400);

    _split_widget = new QSplitter(this);
    _split_widget->insertWidget(0, _up_widget);
    _split_widget->insertWidget(1, _dn_widget);
    _split_widget->setOrientation(Qt::Vertical);
    _split_widget->setCollapsible(0, false);
    _split_widget->setCollapsible(1, false);
    //_split_widget->setStretchFactor(1, 1);
    //_split_widget

    this->setWidgetResizable(true);
    this->setWidget(_split_widget);
    //_split_widget->setGeometry(0, 0, sizeHint().width(), 500);
    _split_widget->setObjectName("protocolWidget");

    connect(&_session, SIGNAL(decode_done()), this, SLOT(update_model()));
    connect(this, SIGNAL(protocol_updated()), this, SLOT(update_model()));
    connect(_table_view, SIGNAL(clicked(QModelIndex)), this, SLOT(item_clicked(QModelIndex)));
    connect(_table_view->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), this, SLOT(column_resize(int, int, int)));
}

ProtocolDock::~ProtocolDock()
{
}

int ProtocolDock::decoder_name_cmp(const void *a, const void *b)
{
    return strcmp(((const srd_decoder*)a)->name,
        ((const srd_decoder*)b)->name);
}

void ProtocolDock::add_protocol()
{
    if (_session.get_device()->dev_inst()->mode != LOGIC) {
        QMessageBox msg(this);
        msg.setText(tr("Protocol Analyzer"));
        msg.setInformativeText(tr("Protocol Analyzer is only valid in Digital Mode!"));
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    } else {
        srd_decoder *const decoder =
            (srd_decoder*)(_protocol_combobox->itemData(_protocol_combobox->currentIndex())).value<void*>();
        if (_session.add_decoder(decoder)) {
            //std::list <int > _sel_probes = dlg.get_sel_probes();
            //QMap <QString, QVariant>& _options = dlg.get_options();
            //QMap <QString, int> _options_index = dlg.get_options_index();

            QPushButton *_del_button = new QPushButton(_up_widget);
            QPushButton *_set_button = new QPushButton(_up_widget);
            _del_button->setFlat(true);
            _del_button->setIcon(QIcon::fromTheme("protocol",
                                 QIcon(":/icons/del.png")));
            _set_button->setFlat(true);
            _set_button->setIcon(QIcon::fromTheme("protocol",
                                 QIcon(":/icons/gear.png")));
            QLabel *_protocol_label = new QLabel(_up_widget);
            QLabel *_progress_label = new QLabel(_up_widget);

            _del_button->setCheckable(true);
            _protocol_label->setText(_protocol_combobox->currentText());

            connect(_del_button, SIGNAL(clicked()),
                    this, SLOT(del_protocol()));
            connect(_set_button, SIGNAL(clicked()),
                    this, SLOT(rst_protocol()));

            _del_button_list.push_back(_del_button);
            _set_button_list.push_back(_set_button);
            _protocol_label_list.push_back(_protocol_label);
            _progress_label_list.push_back(_progress_label);
            _protocol_index_list.push_back(_protocol_combobox->currentIndex());

            QHBoxLayout *hori_layout = new QHBoxLayout();
            hori_layout->addWidget(_set_button);
            hori_layout->addWidget(_del_button);
            hori_layout->addWidget(_protocol_label);
            hori_layout->addWidget(_progress_label);
            hori_layout->addStretch(1);
            _hori_layout_list.push_back(hori_layout);
            _up_layout->insertLayout(_del_button_list.size(), hori_layout);

            // progress connection
            const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
                _session.get_decode_signals());
            connect(decode_sigs.back().get(), SIGNAL(decoded_progress(int)), this, SLOT(decoded_progess(int)));

            protocol_updated();
        }
    }
}

void ProtocolDock::rst_protocol()
{
    int rst_index = 0;
    for (QVector <QPushButton *>::const_iterator i = _set_button_list.begin();
         i != _set_button_list.end(); i++) {
        QPushButton *button = qobject_cast<QPushButton *>(sender());
        if ((*i) == button) {
            //pv::decoder::DemoConfig dlg(this, _session.get_device(), _protocol_index_list.at(rst_index));
            //dlg.set_config(_session.get_decode_probes(rst_index), _session.get_decode_options_index(rst_index));
            //if (dlg.exec()) {
                //std::list <int > _sel_probes = dlg.get_sel_probes();
                //QMap <QString, QVariant>& _options = dlg.get_options();
                //QMap <QString, int> _options_index = dlg.get_options_index();

                //_session.rst_protocol_analyzer(rst_index, _sel_probes, _options, _options_index);
            //}
            _session.rst_decoder(rst_index);
            break;
        }
        rst_index++;
    }
    protocol_updated();
}

void ProtocolDock::del_protocol()
{
    if (_del_all_button->isChecked()) {
        _del_all_button->setChecked(false);
        if (_hori_layout_list.size() > 0) {
            int del_index = 0;
            for (QVector <QHBoxLayout *>::const_iterator i = _hori_layout_list.begin();
                 i != _hori_layout_list.end(); i++) {
                _up_layout->removeItem((*i));
                delete (*i);
                delete _del_button_list.at(del_index);
                delete _set_button_list.at(del_index);
                delete _protocol_label_list.at(del_index);
                delete _progress_label_list.at(del_index);

                _session.remove_decode_signal(0);
                del_index++;
            }
            _hori_layout_list.clear();
            _del_button_list.clear();
            _set_button_list.clear();
            _protocol_label_list.clear();
            _progress_label_list.clear();
            _protocol_index_list.clear();
        } else {
            QMessageBox msg(this);
            msg.setText(tr("Protocol Analyzer"));
            msg.setInformativeText(tr("No Protocol Analyzer to delete!"));
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
        }
    } else {
       int del_index = 0;
       for (QVector <QPushButton *>::const_iterator i = _del_button_list.begin();
            i != _del_button_list.end(); i++) {
           if ((*i)->isChecked()) {
               _up_layout->removeItem(_hori_layout_list.at(del_index));

               delete _hori_layout_list.at(del_index);
               delete _del_button_list.at(del_index);
               delete _set_button_list.at(del_index);
               delete _protocol_label_list.at(del_index);
               delete _progress_label_list.at(del_index);

               _hori_layout_list.remove(del_index);
               _del_button_list.remove(del_index);
               _set_button_list.remove(del_index);
               _protocol_label_list.remove(del_index);
               _progress_label_list.remove(del_index);
               _protocol_index_list.remove(del_index);

               _session.remove_decode_signal(del_index);
               break;
           }
           del_index++;
       }
    }
    protocol_updated();
}

void ProtocolDock::del_all_protocol()
{
    if (_hori_layout_list.size() > 0) {
        int del_index = 0;
        for (QVector <QHBoxLayout *>::const_iterator i = _hori_layout_list.begin();
             i != _hori_layout_list.end(); i++) {
            _up_layout->removeItem((*i));
            delete (*i);
            delete _del_button_list.at(del_index);
            delete _set_button_list.at(del_index);
            delete _protocol_label_list.at(del_index);
            delete _progress_label_list.at(del_index);

            _session.remove_decode_signal(0);
            del_index++;
        }
        _hori_layout_list.clear();
        _del_button_list.clear();
        _set_button_list.clear();
        _protocol_label_list.clear();
        _progress_label_list.clear();
        _protocol_index_list.clear();

        protocol_updated();
    }
}

void ProtocolDock::decoded_progess(int progress)
{
    (void) progress;

    const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    int index = 0;
    BOOST_FOREACH(boost::shared_ptr<pv::view::DecodeTrace> d, decode_sigs) {
        QString progress_str = QString::number(d->get_progress()) + "%";
        if (d->get_progress() == 100)
            _progress_label_list.at(index)->setStyleSheet("color:green;");
        else
            _progress_label_list.at(index)->setStyleSheet("color:red;");
        _progress_label_list.at(index)->setText(progress_str);
        index++;
    }
    update_model();
}

void ProtocolDock::set_model()
{
    pv::dialogs::ProtocolList *protocollist_dlg = new pv::dialogs::ProtocolList(this, _session);
    protocollist_dlg->exec();
    resize_table_view(_session.get_decoder_model());
}

void ProtocolDock::update_model()
{
    pv::data::DecoderModel *decoder_model = _session.get_decoder_model();
    const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    if (decode_sigs.size() == 0)
        decoder_model->setDecoderStack(NULL);
    else if (!decoder_model->getDecoderStack())
        decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    else {
        int index = 0;
        BOOST_FOREACH(const boost::shared_ptr<pv::view::DecodeTrace> d, decode_sigs) {
            if (d->decoder() == decoder_model->getDecoderStack()) {
                decoder_model->setDecoderStack(d->decoder());
                break;
            }
            index++;
        }
        if (index >= decode_sigs.size())
            decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    }

    resize_table_view(decoder_model);
}

void ProtocolDock::resize_table_view(data::DecoderModel* decoder_model)
{
    if (decoder_model->getDecoderStack()) {
        for (int i = 0; i < decoder_model->columnCount(QModelIndex()) - 1; i++) {
            _table_view->resizeColumnToContents(i);
            if (_table_view->columnWidth(i) > 200)
                _table_view->setColumnWidth(i, 200);
        }
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::item_clicked(const QModelIndex &index)
{
    pv::data::DecoderModel *decoder_model = _session.get_decoder_model();
    boost::shared_ptr<pv::data::DecoderStack> decoder_stack = decoder_model->getDecoderStack();
    if (decoder_stack) {
        pv::data::decode::Annotation ann;
        if (decoder_stack->list_annotation(ann, index.column(), index.row())) {
            _session.show_region(ann.start_sample(), ann.end_sample());
        }
    }
}

void ProtocolDock::column_resize(int index, int old_size, int new_size)
{
    pv::data::DecoderModel *decoder_model = _session.get_decoder_model();
    if (decoder_model->getDecoderStack()) {
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::export_table_view()
{
    pv::dialogs::ProtocolExp *protocolexp_dlg = new pv::dialogs::ProtocolExp(this, _session);
    protocolexp_dlg->exec();
}

} // namespace dock
} // namespace pv
