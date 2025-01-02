#!/usr/bin/bash

# check for valid input
if [[ $# -ne 2 ]]; then
  echo "Usage: $0 -i input_file.txt"
  exit 1
fi

input_file=$2

line_no=1
while IFS= read -r line; do
  line=$(echo "$line" | tr -d '\r')
  case $line_no in
    1) use_archive="$line" ;;
    2) allowed_archived_formats=($line) ;;
    3) allowed_languages=($line) ;;
    4) standard_marks="$line" ;;
    5) unmatched_penalty="$line" ;;
    6) working_dir="$line" ;;
    7) id_range="$line" ;;
    8) expected_output_path="$line" ;;
    9) guidelines_penalty="$line" ;;
    10) plagiarism_file="$line" ;;
    11) plagiarism_penalty="$line" ;;
    *) echo "Invalid input on line $line_no" && exit 1 ;;
  esac
  ((line_no++))
done < "$input_file"

IFS=' ' read -r id_start id_end <<< "$id_range"

marks_file="$working_dir/marks.csv"
echo "id,marks,marks_deducted,total_marks,remarks" >"$marks_file"
mkdir -p "$working_dir/issues" "$working_dir/checked"
issues="$working_dir/issues"
checked="$working_dir/checked"
rm -rf "$issues"/* "$checked"/*

evaluate_submission() {
    student_id=$1
    submission=$2
    remarks=""
    obtained_marks=0
    total_marks=0
    marks_cut=0
    marks_deducted=0

    # Unarchive and move files 
    if [[ $use_archive == "true" ]]; then
        if [[ "$submission" == *.* ]]; then
            archive_format=$(basename "$submission" | cut -d. -f2)
            is_allowed=false
            for format in "${allowed_archived_formats[@]}"; do
                if [[ "$format" == "$archive_format" ]]; then
                    is_allowed=true
                    break
                fi
            done

            # issue case #2 (unsupported archive format)
            if [[ "$is_allowed" == false ]]; then
                remarks="issue case #2"
                marks_deducted=$guidelines_penalty  
                total_marks=$(( -marks_deducted ))
                echo "$student_id,0,$marks_deducted,$total_marks,$remarks" >> "$marks_file"
                return
            fi

            mkdir "$working_dir/$student_id"
            inner_dir=""
            case $archive_format in
            zip) unzip -j "$submission" -d "$working_dir/$student_id" > /dev/null 2>&1 &&
                 inner_dir=$(unzip -Z1 "$submission" | head -n 1 | cut -d'/' -f1) ;;
            tar) tar -xf "$submission" -C "$working_dir/$student_id" > /dev/null 2>&1 &&
                 inner_dir=$(tar -tf "$submission" | grep '/$' | head -n 1 | cut -d'/' -f1);;
            rar) unrar e "$submission" "$working_dir/$student_id" > /dev/null 2>&1 &&
                 inner_dir=$(unrar l "$submission" | grep '/$' | head -n 1 | awk '{print $NF}');;
            *) remarks="unsupported archive format" && echo "$student_id,0,0,0,$remarks" >> "$marks_file" && return ;;
            esac

            #issue case #4 (mismatched folder name)
            if [[ $student_id != "$inner_dir" ]]; then
                remarks="$remarks issue case #4"
                marks_deducted=$guidelines_penalty
            fi
            

        else
            # issue case #1 (not archived)
            remarks="issue case #1"
            marks_deducted=$guidelines_penalty
        fi

    else
        mkdir -p "$working_dir/$student_id"
        mv "$submission" "$working_dir/$student_id/"
    fi

    # Check for submission language
    submission_file=""
    for file in "$working_dir/$student_id"/*; do
        extension=$(basename "$file" | cut -d. -f2)
        if [[ "$extension" == "py" ]]; then
            extension="python"
        fi
        if [[ " ${allowed_languages[@]} " =~ " ${extension} " ]]; then
            submission_file="$file"
            break
        fi
    done
     # issue case #3 (unsupported language)
    if [[ -z $submission_file ]]; then
        remarks="issue case #3"
        marks_deducted=$guidelines_penalty
        total_marks=$(( -marks_deducted ))
        mv "$working_dir/$student_id" "$issues/"
        echo "$student_id,0,$marks_deducted,$total_marks,$remarks" >> "$marks_file"
        return
    fi

    # Run the program
    output_file="$working_dir/$student_id/${student_id}_output.txt"
    case $submission_file in
      *.c) gcc "$submission_file" -o "$student_id" && ./"$student_id" > "$output_file" ;;
      *.cpp) g++ "$submission_file" -o "$student_id" && ./"$student_id" > "$output_file" ;;
      *.py) python3 "$submission_file" > "$output_file" ;;
      *.sh) bash "$submission_file" > "$output_file" ;;
      *) remarks="unsupported language" && mv "$submission" "$issues/" && echo "$student_id,0,0,0,$remarks" >> "$marks_file" && return ;;
    esac

    # Compare output and deduct marks
    unmatched_lines=0
    while IFS= read -r line; do
      if ! grep -Fxq "$line" "$output_file"; then
        ((unmatched_lines++))
      fi
    done < "$expected_output_path"
    marks_cut=$((unmatched_lines * unmatched_penalty))

    # Plagiarism check
    is_plagiarised=false
    total_marks_for_plagiarism=0

    if grep -q "$student_id" "$plagiarism_file"; then
    total_marks_for_plagiarism=$((-(standard_marks * plagiarism_penalty / 100)))
    remarks="$remarks plagiarism detected"
    is_plagiarised=true
    fi
    
    obtained_marks=$((standard_marks - marks_cut))
    if [[ "$is_plagiarised" == "true" ]]; then
        total_marks=$total_marks_for_plagiarism;
    else
        total_marks=$((obtained_marks - marks_deducted))
    fi
    
    echo "$student_id,$obtained_marks,$marks_deducted,$total_marks,$remarks" >> "$marks_file"
    mv "$working_dir/$student_id" "$checked/"
  
}


# issue case #5 (out of range)
for submission in "$working_dir"/*; do
  student_id=$(basename "$submission" | cut -d. -f1)
  if [[ $student_id =~ ^[0-9]+$ && ($student_id -lt $id_start || $student_id -gt $id_end) ]]; then
    remarks="issue case #5"
    echo "$student_id,0,0,$total_marks,$remarks" >> "$marks_file"
  fi
done

for student_id in $(seq "$id_start" "$id_end"); do
  echo "Processing student_id: $student_id"
  submission=$(find "$working_dir" -name "${student_id}*" | head -n 1)
  if [[ -n "$submission" ]]; then
    evaluate_submission "$student_id" "$submission"
  else
    remarks="missing submission"
    echo "$student_id,0,0,$total_marks,$remarks" >> "$marks_file"
  fi
done





