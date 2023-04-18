#!/bin/bash

commands_file=~/.my_commands

add_command() {
  local name=$1
  shift
  local command="$*"

  # Remove any existing commands with the same name
  sed -i "/^$name=/d" "$commands_file"

  # Add the new command
  echo "$name=$command" >> "$commands_file"
  
}

show_commands() {
  echo "Available commands:"
  echo "-------------------"
  while IFS='=' read -r name command; do
    echo "$name: $command"
    echo "To run this command, type: run_cmd \"$name\""
    echo ""
  done < "$commands_file"
}

run_cmd() {
  local name="$1"
  local filename="$2"
  local command=$(grep "^$name=" "$commands_file" | cut -d= -f2-)
  if [[ -n "$command" ]]; then
    if [[ -n "$filename" ]]; then
      eval "$command $filename"
    else
      eval "$command"
    fi
  else
    echo "Error: command not found"
  fi
}


