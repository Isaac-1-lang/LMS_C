#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mysql/mysql.h>
#include <gtk/gtk.h>
#include <unistd.h>

#define MAX_QUERY_LEN 512
#define LIB_NAME_LEN 100
#define TITLE_LEN 100
#define ISBN_LEN 20
#define GENRE_LEN 50
#define YEAR_LEN 10
#define SHELF_LEN 30
#define ADDRESS_LEN 200
#define PHONE_LEN 20
#define EMAIL_LEN 100
#define ROLE_LEN 50
#define STATUS_LEN 20
#define DATE_LEN 11
#define BIO_LEN 1000
#define PASSWORD_LEN 50
#define CONTACT_LEN 100

MYSQL *conn;
GtkWidget *main_window;
int current_user_id = -1;
char current_user_type[20] = "";

// Structure definitions
typedef struct {
    GtkWidget *book_id;
    GtkWidget *title;
    GtkWidget *author_id;
    GtkWidget *publisher_id;
    GtkWidget *isbn;
    GtkWidget *genre;
    GtkWidget *year;
    GtkWidget *copies;
    GtkWidget *shelf;
} BookEntryWidgets;

typedef struct {
    GtkWidget *username;
    GtkWidget *password;
} LoginWidgets;

// Forward declarations
void connectDB(void);
void showMessage(GtkWindow *parent, GtkMessageType type, const char *msg);
int isValidInteger(const char *str);
int isNonEmptyString(const char *str, int max_len);
int isValidYear(const char *str);
int isValidDate(const char *str);
int idExistsInTable(const char *table, const char *id);
int validateBookInput(BookEntryWidgets *widgets);
void createInsertBookWindow(void);
void viewBooksGUI(GtkWidget *widget, gpointer parent_window);
void insertBookConsole(void);
void viewBooksConsole(void);
int authenticateUser(const char *username, const char *password, int *user_id, char *user_type);
void runGuiLogin(int argc, char *argv[]);
void runTerminalInterface(void);
static void on_gui_login_clicked(GtkWidget *widget, gpointer data);
int terminalLogin(void);

// Validation functions
int isValidInteger(const char *str) {
    if (!str || !*str) return 0;
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) return 0;
    }
    return atoi(str) > 0;
}

int isNonEmptyString(const char *str, int max_len) {
    return str && *str && strlen(str) <= max_len;
}

int isValidYear(const char *str) {
    if (!isValidInteger(str)) return 0;
    int year = atoi(str);
    return year >= 1800 && year <= 2025;
}

int isValidDate(const char *str) {
    // Expect format: YYYY-MM-DD
    if (strlen(str) != 10 || str[4] != '-' || str[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit(str[i])) return 0;
    }
    int year = atoi(str);
    int month = atoi(str + 5);
    int day = atoi(str + 8);
    return year >= 1800 && year <= 2025 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

int idExistsInTable(const char *table, const char *id) {
    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query), "SELECT id FROM %s WHERE id = %s", table, id);
    if (mysql_query(conn, query)) return 0;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return 0;
    int exists = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return exists;
}

int validateBookInput(BookEntryWidgets *widgets) {
    const char *book_id = gtk_entry_get_text(GTK_ENTRY(widgets->book_id));
    const char *title = gtk_entry_get_text(GTK_ENTRY(widgets->title));
    const char *author_id = gtk_entry_get_text(GTK_ENTRY(widgets->author_id));
    const char *publisher_id = gtk_entry_get_text(GTK_ENTRY(widgets->publisher_id));
    const char *isbn = gtk_entry_get_text(GTK_ENTRY(widgets->isbn));
    const char *genre = gtk_entry_get_text(GTK_ENTRY(widgets->genre));
    const char *year = gtk_entry_get_text(GTK_ENTRY(widgets->year));
    const char *copies = gtk_entry_get_text(GTK_ENTRY(widgets->copies));
    const char *shelf = gtk_entry_get_text(GTK_ENTRY(widgets->shelf));

    if (!isValidInteger(book_id)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Book ID must be a positive integer.");
        return 0;
    }
    if (!isNonEmptyString(title, TITLE_LEN)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Title must be a non-empty string (max 100 chars).");
        return 0;
    }
    if (!isValidInteger(author_id) || !idExistsInTable("Authors", author_id)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Author ID must be a valid, existing ID.");
        return 0;
    }
    if (!isValidInteger(publisher_id) || !idExistsInTable("Publishers", publisher_id)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Publisher ID must be a valid, existing ID.");
        return 0;
    }
    if (!isNonEmptyString(isbn, ISBN_LEN)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "ISBN must be a non-empty string (max 20 chars).");
        return 0;
    }
    if (!isNonEmptyString(genre, GENRE_LEN)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Genre must be a non-empty string (max 50 chars).");
        return 0;
    }
    if (!isValidYear(year)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Year must be a valid year (1800–2025).");
        return 0;
    }
    if (!isValidInteger(copies)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Copies must be a positive integer.");
        return 0;
    }
    if (!isNonEmptyString(shelf, SHELF_LEN)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Shelf location must be a non-empty string (max 30 chars).");
        return 0;
    }
    return 1;
}

// Database connection
void connectDB(void) {
    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, "localhost", "Isaac", "361304olc0012024", "Library_Management", 0, NULL, 0)) {
        fprintf(stderr, "Database connection failed: %s\n", mysql_error(conn));
        exit(1);
    }
}

// Message display
void showMessage(GtkWindow *parent, GtkMessageType type, const char *msg) {
    GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Insert Book (GUI)
void insertBookGUI(GtkWidget *widget, gpointer data) {
    BookEntryWidgets *widgets = (BookEntryWidgets *)data;
    
    if (!validateBookInput(widgets)) {
        return;
    }

    char escaped_title[TITLE_LEN * 2];
    char escaped_isbn[ISBN_LEN * 2];
    char escaped_genre[GENRE_LEN * 2];
    char escaped_year[YEAR_LEN * 2];
    char escaped_shelf[SHELF_LEN * 2];

    mysql_real_escape_string(conn, escaped_title, gtk_entry_get_text(GTK_ENTRY(widgets->title)), 
                            strlen(gtk_entry_get_text(GTK_ENTRY(widgets->title))));
    mysql_real_escape_string(conn, escaped_isbn, gtk_entry_get_text(GTK_ENTRY(widgets->isbn)), 
                            strlen(gtk_entry_get_text(GTK_ENTRY(widgets->isbn))));
    mysql_real_escape_string(conn, escaped_genre, gtk_entry_get_text(GTK_ENTRY(widgets->genre)), 
                            strlen(gtk_entry_get_text(GTK_ENTRY(widgets->genre))));
    mysql_real_escape_string(conn, escaped_year, gtk_entry_get_text(GTK_ENTRY(widgets->year)), 
                            strlen(gtk_entry_get_text(GTK_ENTRY(widgets->year))));
    mysql_real_escape_string(conn, escaped_shelf, gtk_entry_get_text(GTK_ENTRY(widgets->shelf)), 
                            strlen(gtk_entry_get_text(GTK_ENTRY(widgets->shelf))));

    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "INSERT INTO Books (book_id, title, author_id, publisher_id, isbn, genre, "
             "year_published, copies_available, shelf_location) "
             "VALUES (%s, '%s', %s, %s, '%s', '%s', %s, %s, '%s')",
             gtk_entry_get_text(GTK_ENTRY(widgets->book_id)), escaped_title,
             gtk_entry_get_text(GTK_ENTRY(widgets->author_id)),
             gtk_entry_get_text(GTK_ENTRY(widgets->publisher_id)), escaped_isbn,
             escaped_genre, gtk_entry_get_text(GTK_ENTRY(widgets->year)),
             gtk_entry_get_text(GTK_ENTRY(widgets->copies)), escaped_shelf);

    if (mysql_query(conn, query)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to insert book: %s", mysql_error(conn));
        showMessage(NULL, GTK_MESSAGE_ERROR, error_msg);
    } else {
        showMessage(NULL, GTK_MESSAGE_INFO, "Book inserted successfully.");
    }
}

// Create Insert Book Window
void createInsertBookWindow(void) {
    GtkWidget *book_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(book_window), "Insert New Book");
    gtk_window_set_default_size(GTK_WINDOW(book_window), 400, 400);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(book_window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    BookEntryWidgets *widgets = g_new(BookEntryWidgets, 1);
    widgets->book_id = gtk_entry_new();
    widgets->title = gtk_entry_new();
    widgets->author_id = gtk_entry_new();
    widgets->publisher_id = gtk_entry_new();
    widgets->isbn = gtk_entry_new();
    widgets->genre = gtk_entry_new();
    widgets->year = gtk_entry_new();
    widgets->copies = gtk_entry_new();
    widgets->shelf = gtk_entry_new();

    const char *labels[] = {"Book ID:", "Title:", "Author ID:", "Publisher ID:", 
                           "ISBN:", "Genre:", "Year:", "Copies:", "Shelf Location:"};
    GtkWidget *entry_widgets[] = {widgets->book_id, widgets->title, widgets->author_id,
                                 widgets->publisher_id, widgets->isbn, widgets->genre,
                                 widgets->year, widgets->copies, widgets->shelf};

    for (int i = 0; i < 9; i++) {
        GtkWidget *label = gtk_label_new(labels[i]);
        gtk_grid_attach(GTK_GRID(grid), label, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entry_widgets[i], 1, i, 1, 1);
    }

    GtkWidget *insert_button = gtk_button_new_with_label("Insert Book");
    gtk_grid_attach(GTK_GRID(grid), insert_button, 1, 9, 1, 1);
    g_signal_connect(insert_button, "clicked", G_CALLBACK(insertBookGUI), widgets);
    
    g_signal_connect_swapped(book_window, "destroy", G_CALLBACK(g_free), widgets);
    gtk_widget_show_all(book_window);
}

// View Books (GUI)
void viewBooksGUI(GtkWidget *widget, gpointer parent_window) {
    GtkTreeIter iter;
    GtkWidget *view_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(view_window), "View Books");
    gtk_window_set_default_size(GTK_WINDOW(view_window), 800, 400);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(view_window), scrolled_window);

    GtkListStore *store = gtk_list_store_new(9, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree);

    const char *titles[] = {"Book ID", "Title", "Author ID", "Publisher ID", "ISBN",
                            "Genre", "Year", "Copies", "Shelf"};
    for (int i = 0; i < 9; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    }

    char query[] = "SELECT book_id, title, author_id, publisher_id, isbn, genre, year_published, copies_available, shelf_location FROM Books";
    if (mysql_query(conn, query) == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, row[0], 1, row[1], 2, row[2], 3, row[3],
                                   4, row[4], 5, row[5], 6, row[6], 7, row[7], 8, row[8], -1);
            }
            mysql_free_result(res);
        }
    }

    gtk_widget_show_all(view_window);
}

// Insert Book (Console)
void insertBookConsole(void) {
    char book_id[10], title[TITLE_LEN], author_id[10], publisher_id[10], isbn[ISBN_LEN];
    char genre[GENRE_LEN], year[YEAR_LEN], copies[10], shelf[SHELF_LEN];

    printf("Enter Book ID: ");
    fgets(book_id, sizeof(book_id), stdin);
    book_id[strcspn(book_id, "\n")] = 0;
    if (!isValidInteger(book_id)) {
        printf("Error: Book ID must be a positive integer.\n");
        return;
    }

    printf("Enter Title: ");
    fgets(title, sizeof(title), stdin);
    title[strcspn(title, "\n")] = 0;
    if (!isNonEmptyString(title, TITLE_LEN)) {
        printf("Error: Title must be a non-empty string (max 100 chars).\n");
        return;
    }

    printf("Enter Author ID: ");
    fgets(author_id, sizeof(author_id), stdin);
    author_id[strcspn(author_id, "\n")] = 0;
    if (!isValidInteger(author_id) || !idExistsInTable("Authors", author_id)) {
        printf("Error: Author ID must be a valid, existing ID.\n");
        return;
    }

    printf("Enter Publisher ID: ");
    fgets(publisher_id, sizeof(publisher_id), stdin);
    publisher_id[strcspn(publisher_id, "\n")] = 0;
    if (!isValidInteger(publisher_id) || !idExistsInTable("Publishers", publisher_id)) {
        printf("Error: Publisher ID must be a valid, existing ID.\n");
        return;
    }

    printf("Enter ISBN: ");
    fgets(isbn, sizeof(isbn), stdin);
    isbn[strcspn(isbn, "\n")] = 0;
    if (!isNonEmptyString(isbn, ISBN_LEN)) {
        printf("Error: ISBN must be a non-empty string (max 20 chars).\n");
        return;
    }

    printf("Enter Genre: ");
    fgets(genre, sizeof(genre), stdin);
    genre[strcspn(genre, "\n")] = 0;
    if (!isNonEmptyString(genre, GENRE_LEN)) {
        printf("Error: Genre must be a non-empty string (max 50 chars).\n");
        return;
    }

    printf("Enter Year Published: ");
    fgets(year, sizeof(year), stdin);
    year[strcspn(year, "\n")] = 0;
    if (!isValidYear(year)) {
        printf("Error: Year must be a valid year (1800–2025).\n");
        return;
    }

    printf("Enter Copies Available: ");
    fgets(copies, sizeof(copies), stdin);
    copies[strcspn(copies, "\n")] = 0;
    if (!isValidInteger(copies)) {
        printf("Error: Copies must be a positive integer.\n");
        return;
    }

    printf("Enter Shelf Location: ");
    fgets(shelf, sizeof(shelf), stdin);
    shelf[strcspn(shelf, "\n")] = 0;
    if (!isNonEmptyString(shelf, SHELF_LEN)) {
        printf("Error: Shelf location must be a non-empty string (max 30 chars).\n");
        return;
    }

    char escaped_title[TITLE_LEN * 2], escaped_isbn[ISBN_LEN * 2], escaped_genre[GENRE_LEN * 2];
    char escaped_shelf[SHELF_LEN * 2];
    mysql_real_escape_string(conn, escaped_title, title, strlen(title));
    mysql_real_escape_string(conn, escaped_isbn, isbn, strlen(isbn));
    mysql_real_escape_string(conn, escaped_genre, genre, strlen(genre));
    mysql_real_escape_string(conn, escaped_shelf, shelf, strlen(shelf));

    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query),
             "INSERT INTO Books (book_id, title, author_id, publisher_id, isbn, genre, "
             "year_published, copies_available, shelf_location) "
             "VALUES (%s, '%s', %s, %s, '%s', '%s', %s, %s, '%s')",
             book_id, escaped_title, author_id, publisher_id, escaped_isbn,
             escaped_genre, year, copies, escaped_shelf);

    if (mysql_query(conn, query)) {
        printf("Error: Failed to insert book: %s\n", mysql_error(conn));
    } else {
        printf("Book inserted successfully.\n");
    }
}

// View Books (Console)
void viewBooksConsole(void) {
    char query[] = "SELECT book_id, title, author_id, publisher_id, isbn, genre, year_published, copies_available, shelf_location FROM Books";
    if (mysql_query(conn, query)) {
        printf("Error: %s\n", mysql_error(conn));
        return;
    }
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return;
    printf("Books:\n");
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        printf("ID: %s, Title: %s, Author ID: %s, Publisher ID: %s, ISBN: %s, Genre: %s, Year: %s, Copies: %s, Shelf: %s\n",
               row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
    }
    mysql_free_result(res);
}

// Placeholder GUI functions (to be implemented similarly)
void createInsertPublisherWindow(void) { /* Implement similar to createInsertBookWindow */ }
void viewPublishersGUI(GtkWidget *widget, gpointer parent_window) { /* Implement similar to viewBooksGUI */ }
void createInsertMemberWindow(void) { /* Implement */ }
void viewMembersGUI(GtkWidget *widget, gpointer parent_window) { /* Implement */ }
void createInsertStaffWindow(void) { /* Implement */ }
void viewStaffGUI(GtkWidget *widget, gpointer parent_window) { /* Implement */ }
void createInsertBorrowingWindow(void) { /* Implement */ }
void viewBorrowingsGUI(GtkWidget *widget, gpointer parent_window) { /* Implement */ }
void createInsertFineWindow(void) { /* Implement */ }
void viewFinesGUI(GtkWidget *widget, gpointer parent_window) { /* Implement */ }

// Authentication
int authenticateUser(const char *username, const char *password, int *user_id, char *user_type) {
    char escaped_username[LIB_NAME_LEN * 2], escaped_password[PASSWORD_LEN * 2];
    mysql_real_escape_string(conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(conn, escaped_password, password, strlen(password));

    char query[MAX_QUERY_LEN];
    snprintf(query, sizeof(query), "SELECT id FROM Admins WHERE name = '%s' AND password = '%s'",
             escaped_username, escaped_password);
    if (mysql_query(conn, query) == 0) {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res && mysql_num_rows(res) > 0) {
            MYSQL_ROW row = mysql_fetch_row(res);
            *user_id = atoi(row[0]);
            strcpy(user_type, "Admin");
            mysql_free_result(res);
            return 1;
        }
        mysql_free_result(res);
    }
    return 0;
}

// GUI Login
static void on_gui_login_clicked(GtkWidget *widget, gpointer data) {
    printf("Login button clicked\n");
    LoginWidgets *widgets = (LoginWidgets *)data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(widgets->username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(widgets->password));
    printf("Username: %s, Password: %s\n", username, password);

    if (!isNonEmptyString(username, LIB_NAME_LEN) || !isNonEmptyString(password, PASSWORD_LEN)) {
        showMessage(NULL, GTK_MESSAGE_ERROR, "Username and password cannot be empty.");
        return;
    }

    int user_id;
    char user_type[20];
    if (authenticateUser(username, password, &user_id, user_type)) {
        printf("Authentication successful, user_id: %d, user_type: %s\n", user_id, user_type);
        current_user_id = user_id;
        strcpy(current_user_type, user_type);

        // Create a new dashboard window
        GtkWidget *dashboard_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(dashboard_window), "Library Management Dashboard");
        gtk_window_set_default_size(GTK_WINDOW(dashboard_window), 400, 500);
        g_signal_connect(dashboard_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

        GtkWidget *grid = gtk_grid_new();
        gtk_container_add(GTK_CONTAINER(dashboard_window), grid);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

        const char *labels[] = {
            "Insert Book", "View Books", "Insert Publisher", "View Publishers",
            "Insert Member", "View Members", "Insert Staff", "View Staff",
            "Insert Borrowing", "View Borrowings", "Insert Fine", "View Fines"
        };
        void (*callbacks[])(GtkWidget *, gpointer) = {
            (void (*)(GtkWidget *, gpointer))createInsertBookWindow,
            viewBooksGUI,
            (void (*)(GtkWidget *, gpointer))createInsertPublisherWindow,
            viewPublishersGUI,
            (void (*)(GtkWidget *, gpointer))createInsertMemberWindow,
            viewMembersGUI,
            (void (*)(GtkWidget *, gpointer))createInsertStaffWindow,
            viewStaffGUI,
            (void (*)(GtkWidget *, gpointer))createInsertBorrowingWindow,
            viewBorrowingsGUI,
            (void (*)(GtkWidget *, gpointer))createInsertFineWindow,
            viewFinesGUI
        };

        for (int i = 0; i < 12; i++) {
            GtkWidget *button = gtk_button_new_with_label(labels[i]);
            gtk_grid_attach(GTK_GRID(grid), button, 0, i, 1, 1);
            g_signal_connect(button, "clicked", G_CALLBACK(callbacks[i]), dashboard_window);
        }

        printf("Showing dashboard window\n");
        gtk_widget_show_all(dashboard_window);

        // Hide the login window instead of destroying it
        printf("Hiding login window\n");
        gtk_widget_hide(main_window);
    } else {
        printf("Authentication failed\n");
        showMessage(NULL, GTK_MESSAGE_ERROR, "Invalid username or password.");
    }
}
void runGuiLogin(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Library Management Login");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 300, 150);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    g_signal_connect(main_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(main_window), grid);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    LoginWidgets *widgets = g_new(LoginWidgets, 1);
    widgets->username = gtk_entry_new();
    widgets->password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(widgets->password), FALSE);

    GtkWidget *label_username = gtk_label_new("Username:");
    GtkWidget *label_password = gtk_label_new("Password:");
    GtkWidget *login_button = gtk_button_new_with_label("Login");

    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->username, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_password, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->password, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), login_button, 1, 2, 1, 1);

    g_signal_connect(login_button, "clicked", G_CALLBACK(on_gui_login_clicked), widgets);
    g_signal_connect_swapped(main_window, "destroy", G_CALLBACK(g_free), widgets);

    gtk_widget_show_all(main_window);
    gtk_main();
}

// Terminal Login
int terminalLogin(void) {
    char username[LIB_NAME_LEN], password[PASSWORD_LEN];
    printf("Enter Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;
    printf("Enter Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    if (!isNonEmptyString(username, LIB_NAME_LEN) || !isNonEmptyString(password, PASSWORD_LEN)) {
        printf("Error: Username and password cannot be empty.\n");
        return 0;
    }

    int user_id;
    char user_type[20];
    if (authenticateUser(username, password, &user_id, user_type)) {
        current_user_id = user_id;
        strcpy(current_user_type, user_type);
        printf("Login successful. Welcome, %s!\n", username);
        return 1;
    } else {
        printf("Error: Invalid username or password.\n");
        return 0;
    }
}

// Terminal Interface
void runTerminalInterface(void) {
    while (1) {
        printf("\nLibrary Management System (Console Mode)\n");
        printf("1. Insert Book\n");
        printf("2. View Books\n");
        printf("3. Exit\n");
        printf("Enter choice: ");

        char choice[10];
        fgets(choice, sizeof(choice), stdin);
        choice[strcspn(choice, "\n")] = 0;

        if (strcmp(choice, "1") == 0) {
            insertBookConsole();
        } else if (strcmp(choice, "2") == 0) {
            viewBooksConsole();
        } else if (strcmp(choice, "3") == 0) {
            break;
        } else {
            printf("Invalid choice. Try again.\n");
        }
    }
}

// Main function
int main(int argc, char *argv[]) {
    connectDB();

    int use_console = 0;
    if (argc > 1 && strcmp(argv[1], "--console") == 0) {
        use_console = 1;
    } else {
        printf("Select Interface:\n1. GUI\n2. Console\nEnter choice (1 or 2): ");
        char choice[10];
        fgets(choice, sizeof(choice), stdin);
        use_console = (choice[0] == '2');
    }

    if (use_console) {
        if (terminalLogin()) {
            runTerminalInterface();
        }
    } else {
        runGuiLogin(argc, argv);
    }

    mysql_close(conn);
    return 0;
}