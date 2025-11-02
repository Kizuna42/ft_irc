NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I./includes

SRCS_DIR = srcs
OBJS_DIR = objs

SRCS = $(SRCS_DIR)/main.cpp \
       $(SRCS_DIR)/Server.cpp \
       $(SRCS_DIR)/Client.cpp \
       $(SRCS_DIR)/Channel.cpp \
       $(SRCS_DIR)/CommandHandler.cpp \
       $(SRCS_DIR)/Message.cpp \
       $(SRCS_DIR)/commands/auth.cpp \
       $(SRCS_DIR)/commands/channel.cpp \
       $(SRCS_DIR)/commands/message.cpp \
       $(SRCS_DIR)/commands/operator.cpp

OBJS = $(SRCS:$(SRCS_DIR)/%.cpp=$(OBJS_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "✓ $(NAME) compiled successfully"

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJS_DIR)
	@echo "✓ Object files removed"

fclean: clean
	@rm -f $(NAME)
	@echo "✓ $(NAME) removed"

re: fclean all

.PHONY: all clean fclean re
