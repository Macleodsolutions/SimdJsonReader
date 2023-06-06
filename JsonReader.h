#pragma once
#include <queue>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QtCore>
#include <QFileDialog>
#include <QtWidgets/QMainWindow>
#include <QTreeWidgetItem>
#include "simdjson.h"
#include "ui_JsonReader.h"

// JsonReader class, inherits from QMainWindow, and serves as the main class for JSON file reading and parsing.
class JsonReader : public QMainWindow
{
    Q_OBJECT

public:
    // Default constructor and destructor
    JsonReader(QWidget* parent = nullptr);
    ~JsonReader();

private slots:
    // Declaration of Qt slots which correspond to various user interactions
    void on_loadBtn_clicked();               // Triggered when load button is clicked
    void on_treeWidget_itemExpanded(QTreeWidgetItem* item); // Triggered when a tree widget item is expanded
    void on_textEdit_textChanged();          // Triggered when text edit content changes
    void on_searchNextBtn_clicked();         // Triggered when the "search next" button is clicked
    void on_radioCapital_toggled(bool checked); // Triggered when the case sensitive radio button is toggled
    void on_copyBtn_clicked();               // Triggered when the copy button is clicked

private:
    // Declaration of private data members
    Ui::JsonReaderClass ui; // Instance of UI class
    simdjson::dom::parser parser; // Instance of simdjson parser

    // Variables to hold last search text and last matched item in the tree widget
    QString lastSearchText;
    QTreeWidgetItem* lastMatch = nullptr;

    // Map to store relationship between QTreeWidgetItems and their corresponding JSON elements
    QMap<QTreeWidgetItem*, simdjson::dom::element> itemElementMap;

    // Structure to hold the JSON element's value and its associated color for display
    struct JsonElementDisplay {
        std::string value;
        QColor color;
    };

    // Function to get a JsonElementDisplay object for a given JSON element.
    // This function identifies the type of the JSON element and assigns appropriate value and color properties
    // to a JsonElementDisplay object.
    JsonElementDisplay get_json_element_display(simdjson::dom::element value) {
        using namespace simdjson;
        using dom::element_type;
        JsonElementDisplay elementDisplay;
        switch (value.type()) {
        case element_type::INT64:
            elementDisplay.value = std::to_string(value.get<int64_t>().value());
            elementDisplay.color = QColor(0, 0, 255);
            break;
        case element_type::UINT64:
            elementDisplay.value = std::to_string(value.get<uint64_t>().value());
            elementDisplay.color = QColor(0, 0, 255);
            break;
        case element_type::DOUBLE:
            elementDisplay.value = std::to_string(value.get<double>().value());
            elementDisplay.color = QColor(0, 103, 156);
            break;
        case element_type::STRING:
            elementDisplay.value = std::string(value.get<std::string_view>().value());
            elementDisplay.color = QColor(128, 128, 255);
            break;
        case element_type::BOOL:
            elementDisplay.value = value.get<bool>().value() ? "true" : "false";
            elementDisplay.color = QColor(255, 0, 255);
            break;
        case element_type::NULL_VALUE:
            elementDisplay.value = "null";
            elementDisplay.color = QColor(128, 128, 128);
            break;
        case element_type::ARRAY:
            elementDisplay.value = "ARRAY";
            elementDisplay.color = QColor(255, 0, 0);
            break;
        case element_type::OBJECT:
            elementDisplay.value = "OBJECT";
            elementDisplay.color = QColor(48, 186, 143);
            break;
        default:
            elementDisplay.value = "UNKNOWN_TYPE";
            elementDisplay.color = QColor(0, 0, 0);
            break;
        }
        return elementDisplay;
    }

    // Recursive function to copy a QTreeWidgetItem and its children into a QStringList.
    QStringList copy_recursive(QTreeWidgetItem* item) {
        QStringList pairs;
        QString text = item->text(0);
        pairs << text;

        // Recursion for each child of the item
        for (int i = 0; i < item->childCount(); i++) {
            QStringList childPairs = copy_recursive(item->child(i));
            for (const QString& childPair : childPairs) {
                pairs.append(childPair);
            }
        }

        return pairs;
    }

    // Function to populate a QTreeWidgetItem with children items. The children items are created based on the given JSON element.
    void add_children_to_item(QTreeWidgetItem* item, simdjson::dom::element element) {

        // Inner function to add a child item with a key and a value to the parent item.
        auto add_child = [&](const auto& key, const simdjson::dom::element& value) {
            // Get display properties for the JSON element
            JsonReader::JsonElementDisplay elementDisplay = get_json_element_display(value);

            // Create a child item with the key-value pair as its text
            QTreeWidgetItem* child = new QTreeWidgetItem(QStringList() << QString::fromStdString(key + ": " + elementDisplay.value));

            // Set the color of the child item
            child->setForeground(0, QBrush(elementDisplay.color));

            // Add the child item to the parent item
            item->addChild(child);

            // If the JSON element is an array or an object, add it to the item-element map and add a dummy child to it so it can be expanded
            if (value.type() == simdjson::dom::element_type::OBJECT || value.type() == simdjson::dom::element_type::ARRAY) {
                child->addChild(new QTreeWidgetItem());
                itemElementMap.insert(child, value);
            }
        };

        // If the JSON element is an object, add each of its members as a child of the item
        if (element.type() == simdjson::dom::element_type::OBJECT) {
            simdjson::dom::object obj = element;
            for (auto [key, value] : obj) {
                add_child(std::string(key), value);
            }
        }
        // If the JSON element is an array, add each of its elements as a child of the item
        else if (element.type() == simdjson::dom::element_type::ARRAY) {
            simdjson::dom::array arr = element;
            int index = 0;
            for (auto value : arr) {
                add_child(std::to_string(index++), value);
            }
        }
    }

    // Function to search the tree widget for items that contain a given text. 
    // The search can start from the currently selected item or from the beginning.
    bool searchTree(QTreeWidgetItem* item, const QString& searchText, bool startFromCurrent, bool searchArrays = false) {
        std::queue<QTreeWidgetItem*> queue;
        bool started = !startFromCurrent;
        queue.push(item);

        // Check if case sensitivity is turned on
        Qt::CaseSensitivity cs = ui.radioCapital->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;

        // Breadth-first search
        while (!queue.empty()) {
            QTreeWidgetItem* current = queue.front();
            queue.pop();

            // If startFromCurrent is true, skip the items until we reach the last matched item
            if (!started) {
                if (current == lastMatch) {
                    started = true;
                }
            }
            else if (current->text(0).contains(searchText, cs)) {
                // If the item's text contains the search text, select it, expand its parents, scroll the view to it, and end the search
                ui.treeWidget->clearSelection();
                current->setSelected(true);
                QTreeWidgetItem* parent = current->parent();
                while (parent != nullptr) {
                    parent->setExpanded(true);
                    parent = parent->parent();
                }
                ui.treeWidget->scrollToItem(current, QAbstractItemView::PositionAtCenter);
                lastMatch = current;
                return true;
            }

            // Load children if not already loaded
            if (itemElementMap.contains(current)) {
                this->on_treeWidget_itemExpanded(current);
            }

            // Enqueue all children of the current item
            for (int i = 0; i < current->childCount(); ++i) {
                queue.push(current->child(i));
            }
        }

        return false;
    }

};
