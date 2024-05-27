#include <fcntl.h>   
#include <limits.h>  
#include <locale.h>  
#include <magic.h>
#include <pwd.h>      
#include <stdlib.h>
#include <sys/types.h>  
#include <sys/wait.h>    
#include <config.h>

// Feature: ncurses UI
// Description: Utilizes ncurses library for creating a text-based user interface.
#include <ncurses.h>  

// Feature: File / Directory operations (create, delete, rename, copy, moving)
// Description: Provides functionalities to perform various file and directory operations such as create, delete, rename, copy, and move.
// Functions used: rename_file(), delete_file(), copy_files(), move_file(), create_file(), delete_()
#include <stdio.h>    

// Feature: Search Functionality
// Description: Enables users to search for files within the current directory.
// Functions used: search_file()
#include <string.h>   
#include <strings.h>

// Feature: Directory navigation
// Description: Allows users to navigate through directories using arrow keys and enter key.
// Functions used: get_no_files_in_directory(), get_files(), handle_enter(), show_file_info()
#include <dirent.h> 
#include <sys/stat.h>   
#include <unistd.h>    


// Feature: Memory management
// Description: Allocates and frees memory dynamically as needed.
// Functions used: malloc(), free()
#include <stdlib.h>

#define isDir(mode) (S_ISDIR(mode))

// Initializes the ncurses library for creating a text-based user interface.
void init_curses() {
  initscr();
  noecho();
  curs_set(0);
  start_color();
  init_pair(1, DIR_COLOR, 0);
  init_pair(3, STATUS_SELECTED_COLOR, 0);
}

struct stat file_stats;
WINDOW *current_win, *info_win, *path_win;
int selection, maxx, maxy, len = 0, start = 0;
size_t total_files = 0;
directory_t *current_directory_ = NULL;

void init() {
  current_directory_ = (directory_t *)malloc(sizeof(directory_t));
  if (current_directory_ == NULL) {
    printf("Error Occured.\n");
    exit(0);
  }
}

void init_windows() {
  current_win = newwin(maxy, maxx / 2, 0, 0);
  refresh();
  path_win = newwin(2, maxx, maxy, 0);
  refresh();
  info_win = newwin(maxy, maxx / 2, 0, maxx / 2);
  refresh();
  keypad(current_win, TRUE);
}

void refreshWindows() {
  box(current_win, '|', '-');
  box(info_win, '|', '-');
  wrefresh(current_win);
  wrefresh(path_win);
  wrefresh(info_win);
}

typedef struct {
    char* directory;
    char** target;
    int* len;
} ThreadData;

void* scan_directory(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    DIR* dir_;
    struct dirent* dir_entry;

    dir_ = opendir(data->directory);
    if (dir_ == NULL) {
        pthread_exit(NULL);
    }

    while ((dir_entry = readdir(dir_)) != NULL) {
        if (strcmp(dir_entry->d_name, ".") != 0) {
            data->target[(*data->len)++] = strdup(dir_entry->d_name);
        }
    }

    closedir(dir_);
    pthread_exit(NULL);
}

// Scans the specified directory and retrieves the names of all files and directories in it using multiple threads for parallel processing.
int get_files_multithreaded(char* directory, char* target[], int* len) {
    const int NUM_THREADS = 4;
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];
    int *thread_len = malloc(NUM_THREADS * sizeof(int));
    if (thread_len == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        thread_len[i] = 0;
    }

    int segment_size = (*len) / NUM_THREADS;
    int remainder = (*len) % NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; ++i) {
        thread_data[i].directory = directory;
        thread_data[i].target = target;
        thread_data[i].len = &thread_len[i];

        int size = (i == NUM_THREADS - 1) ? segment_size + remainder : segment_size;

        pthread_create(&threads[i], NULL, scan_directory, &thread_data[i]);

        directory += size;
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        (*len) += thread_len[i];
    }

    free(thread_len); 

    return 1;
}

// Gets the number of files in a directory.
int get_no_files_in_directory(char *directory) {
  int len = 0;
  DIR *dir_;
  struct dirent *dir_entry;

  dir_ = opendir(directory);
  if (dir_ == NULL) {
    return -1;
  }

  while ((dir_entry = readdir(dir_)) != NULL) {
    if (strcmp(dir_entry->d_name, ".") != 0) {
      len++;
    }
  }
  closedir(dir_);
  return len;
}

// Retrieves the names of all files in a directory.
int get_files(char *directory, char *target[]) {
  int i = 0;
  DIR *dir_;
  struct dirent *dir_entry;

  dir_ = opendir(directory);
  if (dir_ == NULL) {
    return -1;
  }

  while ((dir_entry = readdir(dir_)) != NULL) {
    if (strcmp(dir_entry->d_name, ".") != 0) {
      target[i++] = strdup(dir_entry->d_name);
    }
  }
  closedir(dir_);
  return 1;
}

// Scrolls up through the list of files in the current window.
void scroll_up() {
  selection--;
  selection = (selection < 0) ? 0 : selection;
  if (len >= maxy - 1)
    if (selection <= start + maxy / 2) {
      if (start == 0)
        wclear(current_win);
      else {
        start--;
        wclear(current_win);
      }
    }
}
// Scrolls down through the list of files in the current window.
void scroll_down() {
  selection++;
  selection = (selection > len - 1) ? len - 1 : selection;
  if (len >= maxy - 1)
    if (selection - 1 > maxy / 2) {
      if (start + maxy - 2 != len) {
        start++;
        wclear(current_win);
      }
    }
}

// Sorts an array of file names alphabetically.
void sort(char *files_[], int n) {
  char temp[1000];
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (strcmp(files_[i], files_[j]) > 0) {
        strcpy(temp, files_[i]);
        strcpy(files_[i], files_[j]);
        strcpy(files_[j], temp);
      }
    }
  }
}

// Checks if a file contains only ASCII characters.
int check_text(char *path) {
  FILE *ptr;
  ptr = fopen(path, "r");
  int c;
  while ((c = fgetc(ptr)) != EOF) {
    if (c < 0 || c > 127) {
      fclose(ptr);
      return 0;
    }
  }
  fclose(ptr);
  return 1;
}

// Reads and displays the contents of a file in the current window.
void read_(char *path) {
  unsigned char buffer[256];
  int ch;
  wclear(current_win);
  wclear(info_win);
  wresize(current_win, maxy, maxx);

  FILE *ptr;
  printf("%s\n", path);
  ptr = fopen(path, "rb");
  if (ptr == NULL) {
    perror("Error");
  }
  int t = 2, pos = 0, lines = 0, count;

  do {
    wmove(current_win, 1, 2);
    wprintw(current_win, "Press \"E\" to Exit (Caps Lock off)");
    if (check_text(path)) {
      count = 0;
      while (fgets(buffer, sizeof(buffer), ptr)) {
        if (count < pos) {
          count++;
          continue;
        }
        wmove(current_win, ++t, 1);
        wprintw(current_win, "%.*s", maxx - 2, buffer);
      }
    } else {
      while (!feof(ptr)) {
        fread(&buffer, sizeof(unsigned char), maxx - 2, ptr);
        wmove(current_win, ++t, 1);
        for (int i = 0; i < maxx - 2; i += 2) {
          wprintw(current_win, "%02x%02x ", (unsigned int)buffer[i],
                  (unsigned int)buffer[i + 1]);
        }
      }
    }
    box(current_win, '|', '-');
    wrefresh(current_win);
    ch = wgetch(current_win);
    wclear(current_win);
    switch (ch) {
      case 259:
        pos = pos == 0 ? 0 : pos - 1;
        break;
      case 258:
        pos++;
        break;
    }
  } while (ch != 'e');

  endwin();
}

// Renames a file.
void rename_file(char *files[]) {
    char new_name[100];
    int i = 0, c;

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Rename to: ");
    wrefresh(path_win);

    while ((c = wgetch(path_win)) != '\n') {
        if (c == 127 || c == 8) { 
            new_name[--i] = '\0';
            i = i < 0 ? 0 : i;
        } else {
            new_name[i++] = c;
            new_name[i] = '\0';
        }
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Rename to: %s", new_name);
        wrefresh(path_win);
    }

    if (i == 0) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Name cannot be empty.");
        wrefresh(path_win);
        wgetch(path_win);
        return;
    }

    char old_path[1000], new_path[1000];
    snprintf(old_path, sizeof(old_path), "%s%s", current_directory_->cwd, files[selection]);
    snprintf(new_path, sizeof(new_path), "%s%s", current_directory_->cwd, new_name);

    if (rename(old_path, new_path) == 0) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Renamed to: %s", new_name);
        wrefresh(path_win);
   
    }
  wgetch(path_win);
}

// Retrieves the parent directory of a given directory.f
char *get_parent_directory(char *cwd) {
  char *a;
  a = strdup(cwd);
  int i = strlen(a) - 1;
  while (a[--i] != '/')
    ;
  a[++i] = '\0';
  return a;
}

// Deletes a file.
void delete_(char *files[]) {
  char curr_path[1000];
  snprintf(curr_path, sizeof(curr_path), "%s%s", current_directory_->cwd,
           files[selection]);
  remove(curr_path);
}

// Prompts the user to confirm file deletion.
void delete_file(char *files[]) {
  int c;
  wclear(path_win);
  wmove(path_win, 1, 0);
  wprintw(path_win, "Are you sure to delete? (y/n)");

LOOP_:
  c = wgetch(path_win);
  wclear(path_win);
  wmove(path_win, 1, 0);
  wprintw(path_win, "Are you sure to delete? (y/n) %c", c);
  switch (c) {
    case 'y':
    case 'Y':
      delete_(files);
      break;
    case 'n':
    case 'N':
      break;
    default:
      goto LOOP_;
      break;
  }
}

// Copies a file to a new location.
void copy_files(char *files[]) {
  char new_path[1000];
  int i = 0, c;
  wclear(path_win);
  wmove(path_win, 1, 0);
  while ((c = wgetch(path_win)) != '\n') {
    if (c == 127 || c == 8) {
      new_path[--i] = '\0';
      i = i < 0 ? 0 : i;
    } else {
      new_path[i++] = c;
      new_path[i] = '\0';
    }
    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "%s", new_path);
  }
  FILE *new_file, *old_file;
  strcat(new_path, files[selection]);
  char curr_path[1000];
  snprintf(curr_path, sizeof(curr_path), "%s%s", current_directory_->cwd,
           files[selection]);
  old_file = fopen(curr_path, "r");
  new_file = fopen(new_path, "a");
  wmove(current_win, 10, 10);
  wprintw(current_win, "%.*s,%s", maxx, curr_path, new_path);
  printf("%s %s", curr_path, new_path);
  while ((c = fgetc(old_file)) != EOF) {
    fprintf(new_file, "%c", c);
  }
  fclose(old_file);
  fclose(new_file);
}

// Moves a file to a new location.
void move_file(char *files[]) {
    char target_directory[1000];
    int i = 0, c;

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Enter target directory: ");
    wrefresh(path_win);

    while ((c = wgetch(path_win)) != '\n') {
        if (c == 127 || c == 8) { 
            target_directory[--i] = '\0';
            i = i < 0 ? 0 : i;
        } else {
            target_directory[i++] = c;
            target_directory[i] = '\0';
        }
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Enter target directory: %s", target_directory);
        wrefresh(path_win);
    }

    if (i == 0) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Directory cannot be empty.");
        wrefresh(path_win);
        wgetch(path_win);
        return;
    }

    if (target_directory[strlen(target_directory) - 1] != '/') {
        strcat(target_directory, "/");
    }

    char new_path[1000];
    snprintf(new_path, sizeof(new_path), "%s%s", target_directory, files[selection]);

    char curr_path[1000];
    snprintf(curr_path, sizeof(curr_path), "%s%s", current_directory_->cwd, files[selection]);

    if (rename(curr_path, new_path) == 0) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "File moved to: %s", new_path);
        wrefresh(path_win);
    } else {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Error moving file.");
        wrefresh(path_win);
    }

    wgetch(path_win);
}

 // Handles the Enter key press action.
void handle_enter(char *files[]) {
  char *temp, *a;
  a = strdup(current_directory_->cwd);
  endwin();
  if (strcmp(files[selection], "..") == 0) {
    start = 0;
    selection = 0;
    strcpy(current_directory_->cwd, current_directory_->parent_dir);
    current_directory_->parent_dir =
        strdup(get_parent_directory(current_directory_->cwd));
  } else {
    temp = malloc(strlen(files[selection]) + 1);
    snprintf(temp, strlen(files[selection]) + 2, "%s", files[selection]);
    strcat(a, temp);
    stat(a, &file_stats);
    if (isDir(file_stats.st_mode)) {
      start = 0;
      selection = 0;
      current_directory_->parent_dir = strdup(current_directory_->cwd);
      strcat(current_directory_->cwd, temp);
      strcat(current_directory_->cwd, "/");
    } else {
      char temp_[1000];
      snprintf(temp_, sizeof(temp_), "%s%s", current_directory_->cwd,
               files[selection]);

      read_(temp_);
    }
  }
  refresh();
}

// Calculates the total size of a directory recursively.
float get_recursive_size_directory(char *path) {
  float directory_size = 0;
  DIR *dir_ = NULL;
  struct dirent *dir_entry;
  struct stat file_stat;
  char temp_path[1000];
  dir_ = opendir(path);
  if (dir_ == NULL) {
    perror(path);
    exit(0);
    return -1;
  }
  while ((dir_entry = readdir(dir_)) != NULL) {
    if (strcmp(dir_entry->d_name, ".") != 0 &&
        strcmp(dir_entry->d_name, "..") != 0) {
      snprintf(temp_path, sizeof(temp_path), "%s/%s", path, dir_entry->d_name);
      if (dir_entry->d_type != DT_DIR) {
        stat(temp_path, &file_stat);
        total_files++;
        directory_size += (float)(file_stat.st_size) / (float)1024;
      } else {
        total_files++;
        directory_size += (float)4;
        directory_size += get_recursive_size_directory(temp_path);
      }
    }
  }
  closedir(dir_);
  return directory_size;
}

// Displays information about a file.
void show_file_info(char *files[]) {
  wmove(info_win, 1, 1);
  char temp_address[1000];
  total_files = 0;
  if (strcmp(files[selection], "..") != 0) {
    snprintf(temp_address, sizeof(temp_address), "%s%s",
             current_directory_->cwd, files[selection]);
    stat(temp_address, &file_stats);

    wprintw(info_win, "Name: %s\n Type: %s\n Size: %.2f KB\n", files[selection],
            isDir(file_stats.st_mode) ? "Folder" : "File",
            isDir(file_stats.st_mode)
                ? get_recursive_size_directory(temp_address)
                : (float)file_stats.st_size / (float)1024);
    wprintw(info_win, " No. Files: %zu\n",
            isDir(file_stats.st_mode) ? total_files : 1);
  } else {
    wprintw(info_win, "Press Enter to go back\n");
  }
}

// Creates a new file.
void create_file() {
    char new_file_name[100];
    int i = 0, c;

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Enter new file name: ");
    wrefresh(path_win);

    while ((c = wgetch(path_win)) != '\n') {
        if (c == 127 || c == 8) {  // Handle backspace
            new_file_name[--i] = '\0';
            i = i < 0 ? 0 : i;
        } else {
            new_file_name[i++] = c;
            new_file_name[i] = '\0';
        }
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Enter new file name: %s", new_file_name);
        wrefresh(path_win);
    }

    if (i == 0) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "File name cannot be empty.");
        wrefresh(path_win);
        wgetch(path_win);
        return;
    }

    char new_file_path[1000];
    snprintf(new_file_path, sizeof(new_file_path), "%s%s", current_directory_->cwd, new_file_name);

    FILE *new_file = fopen(new_file_path, "w");
    if (new_file == NULL) {
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Error creating file.");
        wrefresh(path_win);
        wgetch(path_win);
        return;
    }

    fclose(new_file);
    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "File created: %s", new_file_name);
    wrefresh(path_win);
    wgetch(path_win);
}

// Searches for a file.
void search_file(char *files[], int len) {
    char search_term[100];
    int i = 0, c;

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "Enter search term: ");
    wrefresh(path_win);

    while ((c = wgetch(path_win)) != '\n') {
        if (c == 127 || c == 8) {  
            search_term[--i] = '\0';
            i = i < 0 ? 0 : i;
        } else {
            search_term[i++] = c;
            search_term[i] = '\0';
        }
        wclear(path_win);
        wmove(path_win, 1, 0);
        wprintw(path_win, "Enter search term: %s", search_term);
        wrefresh(path_win);
    }

    for (int j = 0; j < len; j++) {
        if (strstr(files[j], search_term) != NULL) {
            selection = j;  
            if (selection >= maxy) {
                start = selection - maxy / 2;
            } else {
                start = 0;
            }
            return;
        }
    }

    wclear(path_win);
    wmove(path_win, 1, 0);
    wprintw(path_win, "No match found for: %s", search_term);
    wrefresh(path_win);
    wgetch(path_win); 
}

int main() {
    int i = 0;
    init();
    init_curses();
    getcwd(current_directory_->cwd, sizeof(current_directory_->cwd));
    strcat(current_directory_->cwd, "/");
    current_directory_->parent_dir = strdup(get_parent_directory(current_directory_->cwd));
    int ch;
    do {
        len = get_no_files_in_directory(current_directory_->cwd);
        len = len <= 0 ? 1 : len;
        char *files[len], *temp_dir;
        get_files(current_directory_->cwd, files);
        if (selection > len - 1) {
            selection = len - 1;
        }

        getmaxyx(stdscr, maxy, maxx);
        maxy -= 2;
        int t = 0;
        init_windows();

        for (i = start; i < len; i++) {
            if (t == maxy - 1)
                break;
            int size = snprintf(NULL, 0, "%s%s", current_directory_->cwd, files[i]);
            if (i == selection) {
                wattron(current_win, A_STANDOUT);
            } else {
                wattroff(current_win, A_STANDOUT);
            }

            temp_dir = malloc(size + 1);
            snprintf(temp_dir, size + 1, "%s%s", current_directory_->cwd, files[i]);

            stat(temp_dir, &file_stats);
            isDir(file_stats.st_mode) ? wattron(current_win, COLOR_PAIR(1))
                                      : wattroff(current_win, COLOR_PAIR(1));
            wmove(current_win, t + 1, 2);
            wprintw(current_win, "%.*s\n", maxx, files[i]);
            free(temp_dir);
            t++;
        }
        wmove(path_win, 1, 0);
        wprintw(path_win, " %s", current_directory_->cwd);
        show_file_info(files);
        refreshWindows();

        switch ((ch = wgetch(current_win))) {
            case KEY_UP:
            case KEY_NAVUP:
                scroll_up();
                break;
            case KEY_DOWN:
            case KEY_NAVDOWN:
                scroll_down();
                break;
            case KEY_ENTER:
                handle_enter(files);
                break;
            case 'r':
            case 'R':
                rename_file(files);
                break;
            case 'c':
            case 'C':
                copy_files(files);
                break;
            case 'm':
            case 'M':
                move_file(files);
                break;
            case 'd':
            case 'D':
                delete_file(files);
                break;
            case 's':
            case 'S':
                search_file(files, len);
                break;
            case 'n':
            case 'N':
                create_file();
                break;
        }
        for (i = 0; i < len; i++) {
            free(files[i]);
        }
    } while (ch != 'q');
    endwin();
}