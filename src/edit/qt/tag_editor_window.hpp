// SPDX-License-Identifier: GPL-3.0-only

#ifndef INVADER__EDIT__QT__TAG_EDITOR_WINDOW_HPP
#define INVADER__EDIT__QT__TAG_EDITOR_WINDOW_HPP

#include <QMainWindow>
#include <memory>
#include "tag_file.hpp"
#include "tag_tree_widget.hpp"

namespace Invader::Parser {
    struct ParserStruct;
}

namespace Invader::EditQt {
    class TagTreeWindow;

    class TagEditorWindow : public QMainWindow {
        Q_OBJECT
    public:
        /**
         * Instantiate a TagFileDialog
         * @param parent         parent widget
         * @param parent_window  parent window
         */
        TagEditorWindow(QWidget *parent, TagTreeWindow *parent_window, const TagFile &file);

        /**
         * Close event
         * @param event event pointer
         */
        virtual void closeEvent(QCloseEvent *event);

        /**
         * Get the currently open tag file
         * @return tag file
         */
        const TagFile &get_file() const noexcept;

        ~TagEditorWindow();

    private:
        TagTreeWindow *parent_window;

        bool dirty = false;

        void make_dirty(bool dirty);

        bool perform_save();
        bool perform_save_as();
        bool perform_refactor();
        TagFile file;

        Parser::ParserStruct *parser_data;
    };
}

#endif
