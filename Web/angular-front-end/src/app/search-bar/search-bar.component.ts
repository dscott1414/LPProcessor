import { Component, OnInit, ViewChild, ElementRef, EventEmitter, Output } from '@angular/core';
import { UntypedFormControl } from '@angular/forms';
import { DataService } from '../services/data.service';
import { Source } from '../interfaces/source';

@Component({
  selector: 'app-search-bar',
  templateUrl: './search-bar.component.html',
  styleUrls: ['./search-bar.component.css']
})
export class SearchBarComponent implements OnInit {

  titleControl = new UntypedFormControl();
  authorControl = new UntypedFormControl();
  autoCompleteListAuthors: any[] = [ "" ];
  autoCompleteListTitles: any[] = [ ""];
  authorSearchString: string = "";
  authorSelected: string = "";
  titleSearchString: string = "";
  titleSelected: string = "";
  public timeoutAutoCompleteTitleId: any;
  public timeoutAutoCompleteAuthorId: any;

  @ViewChild('autocompleteInputAuthor') autocompleteInputAuthor!: ElementRef;
  @ViewChild('autocompleteInputTitle') autocompleteInputTitle!: ElementRef;
  @Output() onSelectedOption = new EventEmitter();

  constructor(
    public dataService: DataService
  ) { }

  ngOnInit() {
    this.titleControl.valueChanges.subscribe(userInput => {
      this.autoCompleteTitle(userInput);
    })
    this.authorControl.valueChanges.subscribe(userInput => {
      this.autoCompleteAuthor(userInput);
    })
  }

  private autoCompleteTitle(input:string) {
    this.titleSearchString = input;
    clearTimeout(this.timeoutAutoCompleteTitleId);
    this.timeoutAutoCompleteTitleId = setTimeout(() => {
      this.dataService.getFilteredSourcesList(this.authorSearchString,this.titleSearchString).subscribe(sources => {
        console.log("autoCompleteListTitles")
        this.autoCompleteListTitles = sources['response']
      });
    }, 300);
  }

  private autoCompleteAuthor(input:string) {
    this.authorSearchString = input;
    clearTimeout(this.timeoutAutoCompleteAuthorId);
    this.timeoutAutoCompleteAuthorId = setTimeout(() => {
      this.dataService.getFilteredAuthorsList(this.authorSearchString, this.titleSearchString).subscribe(authors => {
        console.log("autoCompleteListAuthors")
        this.autoCompleteListAuthors = authors['response'];
      });
    }, 300);
  }

  // after you clicked an autosuggest option, this function will show the field you want to show in input
  selectValueTitle(selected: Source) {
    let k = selected ? selected.title : selected;
    this.titleSelected = k;
    return k;
  }

  selectValueAuthor(selected: any) {
    let k = selected ? selected.author : selected;
    this.authorSelected = k;
    return k;
  }

  filterTitleEvent(event:any) {
    this.dataService.searchOption.title = event.source.value;
    this.onSelectedOption.emit(this.dataService.searchOption)
    this.focusOnPlaceInputTitle();
  }

  filterAuthorEvent(event:any) {
    this.dataService.searchOption.author = event.source.value;
    this.onSelectedOption.emit(this.dataService.searchOption)
    this.focusOnPlaceInputAuthor();
  }

  // focus the input field and remove any unwanted text.
  focusOnPlaceInputTitle() {
    this.autocompleteInputTitle.nativeElement.focus();
    this.autocompleteInputTitle.nativeElement.value = '';
  }

  focusOnPlaceInputAuthor() {
    this.autocompleteInputAuthor.nativeElement.focus();
    this.autocompleteInputAuthor.nativeElement.value = '';
  }


}
